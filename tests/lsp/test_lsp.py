"""Protocol tests for joy-lsp.

Run with pytest, or directly: python3 test_lsp.py [path/to/joy-lsp].
The binary comes from the first argument or $JOY_LSP.
"""

import json
import os
import subprocess
import sys

JOY_LSP = os.environ.get("JOY_LSP", "build/joy-lsp")


def frame(body):
    data = body if isinstance(body, bytes) else json.dumps(body).encode()
    return b"Content-Length: %d\r\n\r\n" % len(data) + data


SHUTDOWN = {"jsonrpc": "2.0", "id": 999, "method": "shutdown"}
EXIT = {"jsonrpc": "2.0", "method": "exit"}


def run(*bodies, raw=b"", tail=(SHUTDOWN, EXIT)):
    """Send raw bytes, framed messages, then tail; return (exit code, replies)."""
    data = raw + b"".join(frame(b) for b in list(bodies) + list(tail))
    proc = subprocess.run([JOY_LSP], input=data, capture_output=True, timeout=30)
    replies, out = [], proc.stdout
    while out:
        head, _, rest = out.partition(b"\r\n\r\n")
        size = int(head.split(b":")[1])
        replies.append(json.loads(rest[:size]))
        out = rest[size:]
    return proc.returncode, replies


def initialize():
    return {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}}


def did_open(text):
    return {"jsonrpc": "2.0", "method": "textDocument/didOpen",
            "params": {"textDocument": {"uri": "file:///t.joy", "languageId": "joy",
                                        "version": 1, "text": text}}}


def diagnostics(replies):
    return [r["params"]["diagnostics"] for r in replies
            if r.get("method") == "textDocument/publishDiagnostics"]


def assert_alive(code, replies):
    assert code == 0
    assert replies[-1] == {"jsonrpc": "2.0", "id": 999, "result": None}


def test_missing_node_diagnostic():
    # An unterminated definition yields a MISSING node: zero-width range
    code, replies = run(initialize(), did_open("DEFINE f == 1"))
    assert_alive(code, replies)
    ranges = [d["range"] for diags in diagnostics(replies) for d in diags]
    assert any(r["start"] == r["end"] for r in ranges)


def test_deep_nesting_is_rejected():
    code, replies = run(initialize(), b"[" * 100000)
    assert_alive(code, replies)
    assert replies[1]["error"]["code"] == -32700


def test_malformed_json_is_rejected():
    for bad in (b'{"a" 1}', b'{"a": 1} x', b'[1, 2', b'"abc', b"-", b"01", b'"\\q"'):
        code, replies = run(initialize(), bad)
        assert_alive(code, replies)
        assert replies[1]["error"]["code"] == -32700, bad


def test_numbers_follow_json_grammar():
    # id 1e0 is accepted as the integer 1
    code, replies = run(b'{"jsonrpc": "2.0", "id": 1e0, "method": "initialize", "params": {}}')
    assert_alive(code, replies)
    assert replies[0]["id"] == 1


def test_surrogate_pair_decodes_to_utf8():
    # A \ud83d\ude00 pair must become 4-byte UTF-8 (U+1F600), which hover echoes
    open_doc = (b'{"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": '
                b'{"textDocument": {"uri": "file:///t.joy", "languageId": "joy", '
                b'"version": 1, "text": "DEFINE f == \\"\\ud83d\\ude00\\"."}}}')
    hover = {"jsonrpc": "2.0", "id": 2, "method": "textDocument/hover",
             "params": {"textDocument": {"uri": "file:///t.joy"},
                        "position": {"line": 0, "character": 7}}}
    code, replies = run(initialize(), open_doc, hover)
    assert_alive(code, replies)
    [reply] = [r for r in replies if r.get("id") == 2]
    assert "\U0001F600" in reply["result"]["contents"]["value"]



def test_header_framing_variants():
    init = json.dumps(initialize()).encode()
    cases = {
        "lowercase name": b"content-length: %d\r\n\r\n" % len(init) + init,
        "padded value": b"Content-Length:" + b" " * 300 + b"%d\r\n\r\n" % len(init) + init,
        "long other header": b"X-Foo: " + b"a" * 300 + b"\r\n" + frame(init),
        "zero length": b"Content-Length: 0\r\n\r\n" + frame(init),
        # The unread body joins the next header line; the server resyncs on it
        "missing length": b"Content-Type: x\r\n\r\n" + b'{"id": 5}' + frame(init),
    }
    for name, raw in cases.items():
        code, replies = run(raw=raw)
        assert_alive(code, replies)
        assert [r["id"] for r in replies if "result" in r] == [1, 999], name

def hover_at(id_, character):
    return {"jsonrpc": "2.0", "id": id_, "method": "textDocument/hover",
            "params": {"textDocument": {"uri": "file:///t.joy"},
                       "position": {"line": 0, "character": character}}}


def test_requests_before_initialize_are_refused():
    code, replies = run(hover_at(2, 0), did_open("DEFINE f == 1."), initialize())
    assert_alive(code, replies)
    assert replies[0] == {"jsonrpc": "2.0", "id": 2,
                          "error": {"code": -32002, "message": "Server not initialized"}}
    assert diagnostics(replies) == []  # the didOpen was dropped


def test_requests_after_shutdown_are_refused():
    code, replies = run(initialize(), SHUTDOWN, hover_at(2, 0), tail=(EXIT,))
    assert code == 0
    assert replies[-1]["id"] == 2 and replies[-1]["error"]["code"] == -32600


def test_exit_code_depends_on_shutdown():
    assert run(initialize(), tail=(EXIT,))[0] == 1
    assert run(initialize(), tail=())[0] == 1  # EOF without exit
    assert run(initialize())[0] == 0


def test_reopen_and_ranged_change():
    change = {"jsonrpc": "2.0", "method": "textDocument/didChange",
              "params": {"textDocument": {"uri": "file:///t.joy", "version": 3},
                         "contentChanges": [{"range": {"start": {"line": 0, "character": 0},
                                                       "end": {"line": 0, "character": 1}},
                                             "text": "X"}]}}
    code, replies = run(initialize(), did_open("DEFINE a == 1."),
                        did_open("DEFINE bb == 1."), change, hover_at(2, 7))
    assert_alive(code, replies)
    [reply] = [r for r in replies if r.get("id") == 2]
    assert "bb" in json.dumps(reply["result"])  # reopened text, ranged change ignored


if __name__ == "__main__":
    if len(sys.argv) > 1:
        JOY_LSP = sys.argv[1]
    tests = [f for name, f in sorted(globals().items()) if name.startswith("test_")]
    for t in tests:
        t()
        print("ok", t.__name__)

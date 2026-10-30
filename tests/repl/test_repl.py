"""Interactive REPL tests: drive joy through a pseudo-terminal (POSIX only).

Run with pytest, or directly: python3 test_repl.py [path/to/joy].
The binary comes from the first argument or $JOY.
"""

import os
import pty
import re
import select
import shutil
import sys
import tempfile
import time

JOY = os.path.abspath(os.environ.get("JOY", "build/joy"))

PASTE_START, PASTE_END = b"\x1b[200~", b"\x1b[201~"


def session(*keys):
    """Type each key sequence, then return the lines the REPL printed."""
    cwd = tempfile.mkdtemp()  # keep .joy_history out of the source tree
    pid, fd = pty.fork()
    if pid == 0:
        os.chdir(cwd)
        os.execv(JOY, [JOY, "-l"])
    out = b""

    def drain(seconds):
        nonlocal out
        end = time.time() + seconds
        while time.time() < end:
            if select.select([fd], [], [], 0.05)[0]:
                try:
                    chunk = os.read(fd, 65536)
                except OSError:
                    return
                out += chunk
                # Answer cursor-position queries as a terminal would
                for _ in range(chunk.count(b"\x1b[6n")):
                    os.write(fd, b"\x1b[1;1R")

    drain(1.0)
    for k in keys + (b"\x04",):
        os.write(fd, k)
        drain(0.5)
    try:
        os.kill(pid, 9)
    except ProcessLookupError:
        pass
    os.waitpid(pid, 0)
    shutil.rmtree(cwd, ignore_errors=True)
    text = re.sub(rb"\x1b\[[0-9;?]*[A-Za-z]", b"", out).decode(errors="replace")
    lines = (l.rstrip("\r").split("\r")[-1] for l in text.split("\n"))
    return [l for l in lines if l.strip() and "joy>" not in l]


def test_all_statements_on_a_line_run():
    assert session(b"1 . 2 . 3 .\r", b"10 .\r") == ["1", "2", "3", "10"]


def test_definition_then_use_on_one_line():
    assert session(b"DEFINE a == 7. a . a 1 + .\r") == ["7", "8"]


def test_trailing_blanks_and_comments_do_not_block():
    assert session(b"1 .    \r", b"2 . (* note *)\r", b"3 .\r") == ["1", "2", "3"]


def test_missing_final_period_is_supplied():
    assert session(b"1 2 +\r") == ["3"]


def test_error_discards_rest_of_line():
    out = session(b"1 0 / . 99 .\r", b"5 .\r")
    assert out[-1] == "5" and "99" not in out


def test_ctrl_c_clears_line_and_continues():
    assert session(b"1 2\x03", b"3 4 + .\r") == ["7"]


def test_multi_line_paste_runs_every_statement():
    paste = PASTE_START + b"DEFINE sq ==\n  dup *.\n5 sq .\n6 sq ." + PASTE_END
    assert session(paste, b"\r") == ["25", "36"]


if __name__ == "__main__":
    if len(sys.argv) > 1:
        JOY = os.path.abspath(sys.argv[1])
    tests = [f for name, f in sorted(globals().items()) if name.startswith("test_")]
    for t in tests:
        t()
        print("ok", t.__name__)

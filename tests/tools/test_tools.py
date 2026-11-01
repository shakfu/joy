"""Tests for tools/fmt_joy.py and tools/gen_docs.py. Pytest style; also runs directly."""
import pathlib
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import fmt_joy  # noqa: E402
import gen_docs  # noqa: E402


def definitions(source):
    with tempfile.TemporaryDirectory() as tmp:
        path = pathlib.Path(tmp) / "t.joy"
        path.write_text(source)
        parsed = gen_docs.parse_joy_file(path)
    return {d.name: d for s in parsed.sections for d in s.definitions}


def significant_tokens(source):
    skip = (fmt_joy.TokenType.NEWLINE, fmt_joy.TokenType.WHITESPACE)
    return [t.value for t in fmt_joy.tokenize(source) if t.type not in skip]


def test_fmt_keeps_pattern_colon():
    assert "[h : t]" in fmt_joy.format_joy("[[[h : t]] [h]] cases.\n")


def test_fmt_keeps_interpolated_string_whole():
    source = '$"a ${x "b" concat} c" put.\n'
    assert fmt_joy.format_joy(source) == source
    assert significant_tokens(source) == ['$"a ${x "b" concat} c"', "put", "."]


def test_fmt_keeps_one_blank_line():
    source = "1 put.\n\n2 put.\n"
    assert fmt_joy.format_joy(source) == source


def test_fmt_keeps_native_literals_whole():
    source = "v[1 2] m[[1 2] [3 4]] m[].\n"
    assert fmt_joy.format_joy(source) == source


def test_fmt_keeps_numeric_char_escape():
    assert significant_tokens("'\\010 'a '\\n.\n") == ["'\\010", "'a", "'\\n", "."]


def test_fmt_comments_do_not_nest():
    assert significant_tokens("(* a (* b *) c.\n") == ["(* a (* b *)", "c", "."]


def test_fmt_keeps_unknown_characters():
    assert "," in fmt_joy.format_joy("a , b.\n")


def test_fmt_line_comment_unchanged():
    source = "# it's   spaced\n1 put.\n"
    assert fmt_joy.format_joy(source) == source


def test_fmt_preserves_every_joy_file():
    for path in [*ROOT.glob("lib/*.joy"), *ROOT.glob("tests/**/*.joy")]:
        source = path.read_text(errors="surrogateescape")
        formatted = fmt_joy.format_joy(source)
        assert significant_tokens(formatted) == significant_tokens(source), path
        assert fmt_joy.format_joy(formatted) == formatted, path


def test_docs_period_terminated_definition():
    found = definitions("DEFINE a == 1 2 +.\n")
    assert found["a"].body == "1 2 +"


def test_docs_library_definition_map_reduce():
    parsed = gen_docs.parse_joy_file(ROOT / "lib" / "mapreduce.joy")
    names = {d.name for s in parsed.sections for d in s.definitions}
    assert "map-reduce" in names


def test_docs_ignore_definitions_in_comments():
    found = definitions("(* old == 1; *)\n# older == 2;\nb == 3;\n")
    assert set(found) == {"b"}


def test_docs_period_inside_literals_does_not_end_body():
    found = definitions('a == "x. y" 1.5 \'. [b.c] put;\n')
    assert found["a"].body == '"x. y" 1.5 \'. [b.c] put'


def test_docs_comments_do_not_nest():
    assert set(definitions("(* a (* b *)\nc == 1;\n")) == {"c"}


def test_docs_several_definitions_on_one_line():
    assert set(definitions("DEFINE a == 1; b == 2.\n")) == {"a", "b"}


if __name__ == "__main__":
    failures = 0
    for name, test in sorted(globals().items()):
        if name.startswith("test_"):
            try:
                test()
            except AssertionError as exc:
                failures += 1
                print(f"FAIL {name}: {exc}")
    print("FAIL" if failures else "PASS")
    sys.exit(1 if failures else 0)

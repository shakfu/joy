#!/usr/bin/env python3
"""
Joy code formatter - auto-format Joy source files.

Applies consistent formatting:
- Single space between tokens
- No space inside brackets [] or braces {}
- Consistent indentation (4 spaces within LIBRA/DEFINE blocks)
- Preserved comments
- Normalized line endings

Usage:
    python tools/fmt_joy.py file.joy              # Print formatted to stdout
    python tools/fmt_joy.py file.joy -i           # In-place formatting
    python tools/fmt_joy.py lib/*.joy -i          # Format multiple files
    python tools/fmt_joy.py file.joy --check      # Check if formatted (exit 1 if not)
"""
import argparse
import pathlib
import re
import sys
from dataclasses import dataclass
from enum import Enum, auto
from typing import Iterator


class TokenType(Enum):
    """Joy token types."""
    WORD = auto()           # identifier or operator
    NUMBER = auto()         # integer or float
    STRING = auto()         # "..."
    CHAR = auto()           # 'x or '\n
    LBRACKET = auto()       # [
    RBRACKET = auto()       # ]
    LBRACE = auto()         # {
    RBRACE = auto()         # }
    SEMICOLON = auto()      # ;
    DOT = auto()            # .
    DEFINE = auto()         # ==
    COMMENT = auto()        # (* ... *)
    NEWLINE = auto()        # line break
    WHITESPACE = auto()     # spaces/tabs (preserved for indent detection)
    KEYWORD = auto()        # LIBRA, DEFINE, HIDE, etc.


@dataclass
class Token:
    """A Joy token with its type and value."""
    type: TokenType
    value: str
    line: int = 0
    col: int = 0


KEYWORDS = {'LIBRA', 'DEFINE', 'HIDE', 'IN', 'MODULE', 'PRIVATE', 'PUBLIC', 'END', 'CONST'}


def tokenize(source: str) -> Iterator[Token]:
    """Tokenize Joy source code."""
    i = 0
    line = 1
    col = 1
    n = len(source)

    while i < n:
        start_col = col

        # Newline
        if source[i] == '\n':
            yield Token(TokenType.NEWLINE, '\n', line, col)
            i += 1
            line += 1
            col = 1
            continue

        # Whitespace (spaces/tabs)
        if source[i] in ' \t':
            start = i
            while i < n and source[i] in ' \t':
                i += 1
                col += 1
            yield Token(TokenType.WHITESPACE, source[start:i], line, start_col)
            continue

        # Comment (* ... *)
        if i + 1 < n and source[i:i+2] == '(*':
            start = i
            i += 2
            col += 2
            depth = 1
            while i < n and depth > 0:
                if i + 1 < n and source[i:i+2] == '(*':
                    depth += 1
                    i += 2
                    col += 2
                elif i + 1 < n and source[i:i+2] == '*)':
                    depth -= 1
                    i += 2
                    col += 2
                elif source[i] == '\n':
                    i += 1
                    line += 1
                    col = 1
                else:
                    i += 1
                    col += 1
            yield Token(TokenType.COMMENT, source[start:i], line, start_col)
            continue

        # String "..."
        if source[i] == '"':
            start = i
            i += 1
            col += 1
            while i < n and source[i] != '"':
                if source[i] == '\\' and i + 1 < n:
                    i += 2
                    col += 2
                elif source[i] == '\n':
                    i += 1
                    line += 1
                    col = 1
                else:
                    i += 1
                    col += 1
            if i < n:
                i += 1  # closing quote
                col += 1
            yield Token(TokenType.STRING, source[start:i], line, start_col)
            continue

        # Character literal 'x or '\n
        if source[i] == "'":
            start = i
            i += 1
            col += 1
            if i < n and source[i] == '\\':
                i += 1
                col += 1
                if i < n:
                    i += 1
                    col += 1
            elif i < n:
                i += 1
                col += 1
            yield Token(TokenType.CHAR, source[start:i], line, start_col)
            continue

        # Definition ==
        if i + 1 < n and source[i:i+2] == '==':
            yield Token(TokenType.DEFINE, '==', line, start_col)
            i += 2
            col += 2
            continue

        # Single character tokens
        if source[i] == '[':
            yield Token(TokenType.LBRACKET, '[', line, col)
            i += 1
            col += 1
            continue
        if source[i] == ']':
            yield Token(TokenType.RBRACKET, ']', line, col)
            i += 1
            col += 1
            continue
        if source[i] == '{':
            yield Token(TokenType.LBRACE, '{', line, col)
            i += 1
            col += 1
            continue
        if source[i] == '}':
            yield Token(TokenType.RBRACE, '}', line, col)
            i += 1
            col += 1
            continue
        if source[i] == ';':
            yield Token(TokenType.SEMICOLON, ';', line, col)
            i += 1
            col += 1
            continue
        if source[i] == '.':
            yield Token(TokenType.DOT, '.', line, col)
            i += 1
            col += 1
            continue

        # Number (integer or float)
        if source[i].isdigit() or (source[i] == '-' and i + 1 < n and source[i+1].isdigit()):
            start = i
            if source[i] == '-':
                i += 1
                col += 1
            while i < n and source[i].isdigit():
                i += 1
                col += 1
            if i < n and source[i] == '.' and i + 1 < n and source[i+1].isdigit():
                i += 1
                col += 1
                while i < n and source[i].isdigit():
                    i += 1
                    col += 1
            # Scientific notation
            if i < n and source[i] in 'eE':
                i += 1
                col += 1
                if i < n and source[i] in '+-':
                    i += 1
                    col += 1
                while i < n and source[i].isdigit():
                    i += 1
                    col += 1
            yield Token(TokenType.NUMBER, source[start:i], line, start_col)
            continue

        # Word (identifier or operator)
        if source[i].isalnum() or source[i] in '_-+*/<>=!@#$%^&|~?':
            start = i
            # Include alphanumeric and common Joy operator chars
            while i < n and (source[i].isalnum() or source[i] in '_-+*/<>=!@#$%^&|~?'):
                i += 1
                col += 1
            word = source[start:i]
            if word in KEYWORDS:
                yield Token(TokenType.KEYWORD, word, line, start_col)
            else:
                yield Token(TokenType.WORD, word, line, start_col)
            continue

        # Skip unknown characters
        i += 1
        col += 1


def format_joy(source: str, indent: str = "    ") -> str:
    """Format Joy source code.

    Formatting rules:
    - Single space between tokens
    - No space immediately after [ or {
    - No space immediately before ] or }
    - Keywords (LIBRA, DEFINE, etc.) on their own line
    - Definitions indented within blocks
    - Multi-line definitions: continuation lines get extra indent
    - Comments preserved with proper spacing
    """
    tokens = list(tokenize(source))
    result = []

    i = 0
    n = len(tokens)
    in_block = False  # Inside LIBRA/DEFINE/HIDE block
    block_depth = 0
    at_line_start = True
    in_definition_body = False
    definition_start_line = 0
    current_line = 1

    def emit(s: str):
        nonlocal at_line_start
        result.append(s)
        at_line_start = s.endswith('\n')

    def emit_newline():
        nonlocal at_line_start, current_line
        if result and result[-1] != '\n':
            result.append('\n')
        at_line_start = True
        current_line += 1

    def needs_space_before():
        """Check if we need a space before the next token."""
        if not result or at_line_start:
            return False
        last = result[-1]
        return last not in (' ', '\n', '[', '{')

    def emit_base_indent():
        """Emit base indentation for current block depth."""
        if at_line_start and in_block:
            result.append(indent)

    def emit_continuation_indent():
        """Emit extra indent for definition continuation lines."""
        if at_line_start and in_definition_body and current_line > definition_start_line:
            result.append(indent)

    while i < n:
        tok = tokens[i]

        # Handle newlines - preserve blank lines
        if tok.type == TokenType.NEWLINE:
            emit_newline()
            i += 1
            # Skip whitespace but count consecutive newlines
            blank_lines = 0
            while i < n and tokens[i].type in (TokenType.WHITESPACE, TokenType.NEWLINE):
                if tokens[i].type == TokenType.NEWLINE:
                    blank_lines += 1
                i += 1
            # Preserve up to one blank line between definitions
            if blank_lines > 0 and not in_definition_body:
                emit_newline()
            continue

        # Skip original whitespace
        if tok.type == TokenType.WHITESPACE:
            i += 1
            continue

        # Comments - preserve with proper spacing
        if tok.type == TokenType.COMMENT:
            if at_line_start:
                emit_base_indent()
            elif needs_space_before():
                result.append(' ')
            emit(tok.value)
            i += 1
            continue

        # Keywords that start/end blocks
        if tok.type == TokenType.KEYWORD:
            if tok.value in ('LIBRA', 'HIDE', 'PRIVATE', 'PUBLIC', 'MODULE', 'CONST'):
                in_block = True
                block_depth += 1
            elif tok.value in ('END', 'IN'):
                if tok.value == 'END':
                    block_depth = max(0, block_depth - 1)
                    if block_depth == 0:
                        in_block = False
            if not at_line_start:
                emit_newline()
            emit(tok.value)
            i += 1
            continue

        # Definition operator ==
        if tok.type == TokenType.DEFINE:
            if needs_space_before():
                result.append(' ')
            emit('==')
            in_definition_body = True
            definition_start_line = current_line
            i += 1
            continue

        # Semicolon ends definition
        if tok.type == TokenType.SEMICOLON:
            emit(';')
            in_definition_body = False
            i += 1
            continue

        # Dot (statement terminator)
        if tok.type == TokenType.DOT:
            emit('.')
            in_definition_body = False
            i += 1
            continue

        # Opening brackets - space before but not after
        if tok.type == TokenType.LBRACKET:
            if at_line_start:
                emit_base_indent()
                emit_continuation_indent()
            elif needs_space_before():
                result.append(' ')
            emit('[')
            i += 1
            continue

        if tok.type == TokenType.LBRACE:
            if at_line_start:
                emit_base_indent()
                emit_continuation_indent()
            elif needs_space_before():
                result.append(' ')
            emit('{')
            i += 1
            continue

        # Closing brackets - no space before
        if tok.type == TokenType.RBRACKET:
            emit(']')
            i += 1
            continue

        if tok.type == TokenType.RBRACE:
            emit('}')
            i += 1
            continue

        # Regular tokens (words, numbers, strings, chars)
        if at_line_start:
            emit_base_indent()
            emit_continuation_indent()
        elif needs_space_before():
            result.append(' ')

        emit(tok.value)
        i += 1

    # Ensure file ends with newline
    if result and result[-1] != '\n':
        result.append('\n')

    return ''.join(result)


def main():
    parser = argparse.ArgumentParser(
        description="Format Joy source files"
    )
    parser.add_argument(
        "files",
        nargs="+",
        type=pathlib.Path,
        help="Joy files to format"
    )
    parser.add_argument(
        "-i", "--in-place",
        action="store_true",
        help="Edit files in place"
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Check if files are formatted (exit 1 if not)"
    )
    parser.add_argument(
        "--indent",
        default="    ",
        help="Indentation string (default: 4 spaces)"
    )
    parser.add_argument(
        "--diff",
        action="store_true",
        help="Show diff instead of formatted output"
    )
    args = parser.parse_args()

    exit_code = 0

    for path in args.files:
        if not path.exists():
            print(f"Error: {path} not found", file=sys.stderr)
            exit_code = 1
            continue

        if path.suffix != '.joy':
            print(f"Warning: {path} is not a .joy file, skipping", file=sys.stderr)
            continue

        try:
            original = path.read_text()
            formatted = format_joy(original, indent=args.indent)

            if args.check:
                if original != formatted:
                    print(f"{path}: needs formatting", file=sys.stderr)
                    exit_code = 1
                else:
                    print(f"{path}: ok")
            elif args.diff:
                if original != formatted:
                    import difflib
                    diff = difflib.unified_diff(
                        original.splitlines(keepends=True),
                        formatted.splitlines(keepends=True),
                        fromfile=str(path),
                        tofile=str(path) + " (formatted)"
                    )
                    sys.stdout.writelines(diff)
            elif args.in_place:
                if original != formatted:
                    path.write_text(formatted)
                    print(f"Formatted {path}", file=sys.stderr)
                else:
                    print(f"{path}: no changes", file=sys.stderr)
            else:
                print(formatted, end='')

        except Exception as e:
            print(f"Error processing {path}: {e}", file=sys.stderr)
            exit_code = 1

    return exit_code


if __name__ == "__main__":
    sys.exit(main())

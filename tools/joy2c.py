#!/usr/bin/env python3
"""
joy2c.py - Joy to C code generator

Compiles Joy programs to C code that links against libjoy for maximum performance.

Usage:
    python tools/joy2c.py input.joy [-o output.c] [--inline] [--fold]

Options:
    -o, --output    Output C file (default: input.c)
    --inline        Inline simple operations (dup, swap, +, -, etc.)
    --fold          Fold constant expressions at compile time
    --no-checks     Omit runtime parameter checks (faster but unsafe)

The generated C code:
- Calls Joy builtins directly (no dictionary lookup)
- Can inline simple stack/arithmetic operations
- Links against libjoy (the Joy interpreter library)
- Compiles with: gcc -O3 output.c -I../include -L../build -ljoy
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from enum import Enum, auto
from pathlib import Path
from typing import Any


# =============================================================================
# Lexer
# =============================================================================

class TokenType(Enum):
    INTEGER = auto()
    FLOAT = auto()
    STRING = auto()
    CHAR = auto()
    BOOLEAN = auto()
    SYMBOL = auto()
    LBRACKET = auto()
    RBRACKET = auto()
    LBRACE = auto()
    RBRACE = auto()
    DEFINE = auto()      # ==
    SEMICOLON = auto()   # ; (definition terminator)
    PERIOD = auto()      # . (also definition terminator in DEFINE/LIBRA mode)
    EOF = auto()


@dataclass
class Token:
    type: TokenType
    value: Any
    line: int
    col: int


class Lexer:
    """Tokenize Joy source code."""

    def __init__(self, source: str):
        self.source = source
        self.pos = 0
        self.line = 1
        self.col = 1

    def peek(self, offset: int = 0) -> str:
        pos = self.pos + offset
        if pos >= len(self.source):
            return ""
        return self.source[pos]

    def advance(self) -> str:
        ch = self.peek()
        self.pos += 1
        if ch == "\n":
            self.line += 1
            self.col = 1
        else:
            self.col += 1
        return ch

    def skip_whitespace_and_comments(self) -> None:
        while self.pos < len(self.source):
            ch = self.peek()
            if ch in " \t\n\r":
                self.advance()
            elif ch == "#":
                # Line comment
                while self.peek() and self.peek() != "\n":
                    self.advance()
            elif ch == "(" and self.peek(1) == "*":
                # Block comment (* ... *)
                self.advance()  # (
                self.advance()  # *
                while self.pos < len(self.source):
                    if self.peek() == "*" and self.peek(1) == ")":
                        self.advance()  # *
                        self.advance()  # )
                        break
                    self.advance()
            else:
                break

    def scan_string(self) -> str:
        """Scan a double-quoted string."""
        self.advance()  # Opening "
        result = []
        while self.peek() and self.peek() != '"':
            ch = self.advance()
            if ch == "\\":
                esc = self.advance()
                if esc == "n":
                    result.append("\n")
                elif esc == "t":
                    result.append("\t")
                elif esc == "r":
                    result.append("\r")
                elif esc == "\\":
                    result.append("\\")
                elif esc == '"':
                    result.append('"')
                else:
                    result.append(esc)
            else:
                result.append(ch)
        self.advance()  # Closing "
        return "".join(result)

    def scan_char(self) -> str:
        """Scan a character literal 'x or '\\n."""
        self.advance()  # '
        if self.peek() == "\\":
            self.advance()
            esc = self.advance()
            if esc == "n":
                return "\n"
            elif esc == "t":
                return "\t"
            elif esc == "r":
                return "\r"
            elif esc == "\\":
                return "\\"
            return esc
        return self.advance()

    def scan_number(self) -> Token:
        """Scan an integer or float."""
        start_line, start_col = self.line, self.col
        chars = []

        # Optional sign
        if self.peek() in "+-":
            chars.append(self.advance())

        # Integer part
        while self.peek().isdigit():
            chars.append(self.advance())

        # Check for float
        is_float = False
        if self.peek() == "." and self.peek(1).isdigit():
            is_float = True
            chars.append(self.advance())  # .
            while self.peek().isdigit():
                chars.append(self.advance())

        # Exponent
        if self.peek() in "eE":
            is_float = True
            chars.append(self.advance())
            if self.peek() in "+-":
                chars.append(self.advance())
            while self.peek().isdigit():
                chars.append(self.advance())

        num_str = "".join(chars)
        if is_float:
            return Token(TokenType.FLOAT, float(num_str), start_line, start_col)
        return Token(TokenType.INTEGER, int(num_str), start_line, start_col)

    def scan_symbol(self) -> str:
        """Scan a symbol (identifier or operator)."""
        chars = []
        while self.peek() and not self.peek() in " \t\n\r[]{}();#\"'.":
            # Check for block comment start
            if self.peek() == "(" and self.peek(1) == "*":
                break
            chars.append(self.advance())
        return "".join(chars)

    def next_token(self) -> Token:
        """Get the next token."""
        self.skip_whitespace_and_comments()

        if self.pos >= len(self.source):
            return Token(TokenType.EOF, None, self.line, self.col)

        start_line, start_col = self.line, self.col
        ch = self.peek()

        # Brackets
        if ch == "[":
            self.advance()
            return Token(TokenType.LBRACKET, "[", start_line, start_col)
        if ch == "]":
            self.advance()
            return Token(TokenType.RBRACKET, "]", start_line, start_col)
        if ch == "{":
            self.advance()
            return Token(TokenType.LBRACE, "{", start_line, start_col)
        if ch == "}":
            self.advance()
            return Token(TokenType.RBRACE, "}", start_line, start_col)

        # Semicolon
        if ch == ";":
            self.advance()
            return Token(TokenType.SEMICOLON, ";", start_line, start_col)

        # Period (put operator or definition terminator)
        if ch == ".":
            self.advance()
            return Token(TokenType.SYMBOL, ".", start_line, start_col)

        # String
        if ch == '"':
            return Token(TokenType.STRING, self.scan_string(), start_line, start_col)

        # Character
        if ch == "'":
            return Token(TokenType.CHAR, self.scan_char(), start_line, start_col)

        # Number (including negative)
        if ch.isdigit() or (ch in "+-" and self.peek(1).isdigit()):
            return self.scan_number()

        # Symbol or keyword
        sym = self.scan_symbol()

        # Check for definition operator
        if sym == "==":
            return Token(TokenType.DEFINE, "==", start_line, start_col)

        # Boolean literals
        if sym == "true":
            return Token(TokenType.BOOLEAN, True, start_line, start_col)
        if sym == "false":
            return Token(TokenType.BOOLEAN, False, start_line, start_col)

        # Skip DEFINE and LIBRA keywords (just markers for definition mode)
        if sym in ("DEFINE", "LIBRA"):
            return self.next_token()

        return Token(TokenType.SYMBOL, sym, start_line, start_col)

    def tokenize(self) -> list[Token]:
        """Tokenize the entire source."""
        tokens = []
        while True:
            tok = self.next_token()
            tokens.append(tok)
            if tok.type == TokenType.EOF:
                break
        return tokens


# =============================================================================
# AST
# =============================================================================

@dataclass
class ASTNode:
    """Base class for AST nodes."""
    pass


@dataclass
class IntegerNode(ASTNode):
    value: int


@dataclass
class FloatNode(ASTNode):
    value: float


@dataclass
class StringNode(ASTNode):
    value: str


@dataclass
class CharNode(ASTNode):
    value: str


@dataclass
class BooleanNode(ASTNode):
    value: bool


@dataclass
class SymbolNode(ASTNode):
    name: str


@dataclass
class SetNode(ASTNode):
    members: list[int]


@dataclass
class QuotationNode(ASTNode):
    terms: list[ASTNode] = field(default_factory=list)


@dataclass
class DefinitionNode(ASTNode):
    name: str
    body: QuotationNode


@dataclass
class ProgramNode(ASTNode):
    definitions: list[DefinitionNode] = field(default_factory=list)
    main: QuotationNode = field(default_factory=QuotationNode)


# =============================================================================
# Parser
# =============================================================================

class Parser:
    """Parse Joy tokens into an AST."""

    def __init__(self, tokens: list[Token]):
        self.tokens = tokens
        self.pos = 0

    def peek(self) -> Token:
        if self.pos >= len(self.tokens):
            return self.tokens[-1]  # EOF
        return self.tokens[self.pos]

    def advance(self) -> Token:
        tok = self.peek()
        self.pos += 1
        return tok

    def expect(self, type_: TokenType) -> Token:
        tok = self.advance()
        if tok.type != type_:
            raise SyntaxError(f"Expected {type_}, got {tok.type} at line {tok.line}")
        return tok

    def parse_set(self) -> SetNode:
        """Parse a set literal {1 2 3}."""
        self.expect(TokenType.LBRACE)
        members = []
        while self.peek().type not in (TokenType.RBRACE, TokenType.EOF):
            tok = self.expect(TokenType.INTEGER)
            members.append(tok.value)
        self.expect(TokenType.RBRACE)
        return SetNode(members=members)

    def parse_quotation(self) -> QuotationNode:
        """Parse a quotation [...]."""
        self.expect(TokenType.LBRACKET)
        terms = []
        while self.peek().type not in (TokenType.RBRACKET, TokenType.EOF):
            terms.append(self.parse_term())
        self.expect(TokenType.RBRACKET)
        return QuotationNode(terms=terms)

    def parse_term(self) -> ASTNode:
        """Parse a single term."""
        tok = self.peek()

        if tok.type == TokenType.INTEGER:
            self.advance()
            return IntegerNode(value=tok.value)

        if tok.type == TokenType.FLOAT:
            self.advance()
            return FloatNode(value=tok.value)

        if tok.type == TokenType.STRING:
            self.advance()
            return StringNode(value=tok.value)

        if tok.type == TokenType.CHAR:
            self.advance()
            return CharNode(value=tok.value)

        if tok.type == TokenType.BOOLEAN:
            self.advance()
            return BooleanNode(value=tok.value)

        if tok.type == TokenType.LBRACKET:
            return self.parse_quotation()

        if tok.type == TokenType.LBRACE:
            return self.parse_set()

        if tok.type == TokenType.SYMBOL:
            self.advance()
            return SymbolNode(name=tok.value)

        raise SyntaxError(f"Unexpected token {tok.type} at line {tok.line}")

    def parse_definition(self, name: str) -> DefinitionNode:
        """Parse a definition: name == body ; or name == body."""
        self.expect(TokenType.DEFINE)
        terms = []
        # Parse body until we hit ; or . (as terminator)
        # Note: In Joy's DEFINE/LIBRA syntax, . always ends the definition
        # (you cannot use . as the put operator inside a DEFINE)
        while self.peek().type not in (TokenType.SEMICOLON, TokenType.EOF):
            tok = self.peek()
            # . is always a terminator in DEFINE mode
            if tok.type == TokenType.SYMBOL and tok.value == ".":
                self.advance()  # consume the .
                break
            terms.append(self.parse_term())
        if self.peek().type == TokenType.SEMICOLON:
            self.advance()  # consume ;
        return DefinitionNode(name=name, body=QuotationNode(terms=terms))

    def parse(self) -> ProgramNode:
        """Parse the entire program."""
        program = ProgramNode()

        while self.peek().type != TokenType.EOF:
            tok = self.peek()

            # Check for definition
            if tok.type == TokenType.SYMBOL:
                # Look ahead for ==
                if self.pos + 1 < len(self.tokens) and \
                   self.tokens[self.pos + 1].type == TokenType.DEFINE:
                    name = self.advance().value
                    defn = self.parse_definition(name)
                    program.definitions.append(defn)
                    continue

            # Otherwise it's part of main
            program.main.terms.append(self.parse_term())

        return program


# =============================================================================
# Optimizer
# =============================================================================

class Optimizer:
    """Optimize the AST before code generation."""

    def __init__(self, fold_constants: bool = True, inline: bool = True):
        self.fold_constants = fold_constants
        self.inline = inline
        # Symbols that can be constant-folded (pure, no side effects)
        self.foldable = {
            "+", "-", "*", "/", "%", "div", "rem",
            "neg", "abs", "succ", "pred", "sign",
            "and", "or", "not", "xor",
            "<", ">", "<=", ">=", "=", "!=",
            "dup", "pop", "swap",
            "null", "size", "first", "rest",
            "cons", "swons", "concat",
        }

    def optimize(self, program: ProgramNode) -> ProgramNode:
        """Optimize the program."""
        if self.fold_constants:
            program = self.fold(program)
        return program

    def fold(self, program: ProgramNode) -> ProgramNode:
        """Fold constant expressions."""
        # For now, just fold simple cases in the main body
        program.main = self.fold_quotation(program.main)
        for defn in program.definitions:
            defn.body = self.fold_quotation(defn.body)
        return program

    def fold_quotation(self, quot: QuotationNode) -> QuotationNode:
        """Fold constants in a quotation."""
        result = []
        stack: list[ASTNode] = []

        for term in quot.terms:
            # Recursively fold nested quotations
            if isinstance(term, QuotationNode):
                term = self.fold_quotation(term)

            # Try to evaluate on compile-time stack
            folded = self.try_fold(stack, term)
            if folded is None:
                # Can't fold - flush stack and add term
                result.extend(stack)
                stack = []
                result.append(term)
            # If folded, stack was modified in place

        # Flush remaining stack
        result.extend(stack)
        return QuotationNode(terms=result)

    def try_fold(self, stack: list[ASTNode], term: ASTNode) -> bool | None:
        """Try to fold term into stack. Returns True if folded, None if not."""
        # Push literals onto compile-time stack
        if isinstance(term, (IntegerNode, FloatNode, BooleanNode, CharNode)):
            stack.append(term)
            return True

        if not isinstance(term, SymbolNode):
            return None

        name = term.name

        # Binary integer operations
        if name in ("+", "-", "*", "/", "%", "rem"):
            if len(stack) >= 2:
                b, a = stack[-1], stack[-2]
                if isinstance(a, IntegerNode) and isinstance(b, IntegerNode):
                    stack.pop()
                    stack.pop()
                    if name == "+":
                        stack.append(IntegerNode(a.value + b.value))
                    elif name == "-":
                        stack.append(IntegerNode(a.value - b.value))
                    elif name == "*":
                        stack.append(IntegerNode(a.value * b.value))
                    elif name in ("/", "div") and b.value != 0:
                        stack.append(IntegerNode(a.value // b.value))
                    elif name in ("%", "rem") and b.value != 0:
                        stack.append(IntegerNode(a.value % b.value))
                    else:
                        return None
                    return True
            return None

        # Unary operations
        if name == "neg":
            if stack and isinstance(stack[-1], IntegerNode):
                stack[-1] = IntegerNode(-stack[-1].value)
                return True
            return None

        if name == "succ":
            if stack and isinstance(stack[-1], IntegerNode):
                stack[-1] = IntegerNode(stack[-1].value + 1)
                return True
            return None

        if name == "pred":
            if stack and isinstance(stack[-1], IntegerNode):
                stack[-1] = IntegerNode(stack[-1].value - 1)
                return True
            return None

        if name == "abs":
            if stack and isinstance(stack[-1], IntegerNode):
                stack[-1] = IntegerNode(abs(stack[-1].value))
                return True
            return None

        # Stack operations
        if name == "dup":
            if stack:
                stack.append(stack[-1])
                return True
            return None

        if name == "pop":
            if stack:
                stack.pop()
                return True
            return None

        if name == "swap":
            if len(stack) >= 2:
                stack[-1], stack[-2] = stack[-2], stack[-1]
                return True
            return None

        # Comparison operations
        if name in ("<", ">", "<=", ">=", "=", "!="):
            if len(stack) >= 2:
                b, a = stack[-1], stack[-2]
                if isinstance(a, IntegerNode) and isinstance(b, IntegerNode):
                    stack.pop()
                    stack.pop()
                    if name == "<":
                        result = a.value < b.value
                    elif name == ">":
                        result = a.value > b.value
                    elif name == "<=":
                        result = a.value <= b.value
                    elif name == ">=":
                        result = a.value >= b.value
                    elif name == "=":
                        result = a.value == b.value
                    elif name == "!=":
                        result = a.value != b.value
                    stack.append(BooleanNode(result))
                    return True
            return None

        # Boolean operations
        if name == "not":
            if stack and isinstance(stack[-1], BooleanNode):
                stack[-1] = BooleanNode(not stack[-1].value)
                return True
            return None

        if name in ("and", "or"):
            if len(stack) >= 2:
                b, a = stack[-1], stack[-2]
                if isinstance(a, BooleanNode) and isinstance(b, BooleanNode):
                    stack.pop()
                    stack.pop()
                    if name == "and":
                        stack.append(BooleanNode(a.value and b.value))
                    else:
                        stack.append(BooleanNode(a.value or b.value))
                    return True
            return None

        return None


# =============================================================================
# C Code Emitter
# =============================================================================

class CEmitter:
    """Generate C code from Joy AST."""

    # Map Joy symbol names to C function names
    BUILTIN_MAP = {
        # Arithmetic
        "+": "plus_", "-": "minus_", "*": "mul_", "/": "divide_",
        "%": "rem_", "div": "div_", "rem": "rem_",
        "neg": "neg_", "abs": "abs_", "succ": "succ_", "pred": "pred_",
        "sign": "sign_", "max": "max_", "min": "min_",
        "ceil": "ceil_", "floor": "floor_", "trunc": "trunc_", "round": "round_",

        # Math
        "sin": "sin_", "cos": "cos_", "tan": "tan_",
        "asin": "asin_", "acos": "acos_", "atan": "atan_", "atan2": "atan2_",
        "sinh": "sinh_", "cosh": "cosh_", "tanh": "tanh_",
        "exp": "exp_", "log": "log_", "log10": "log10_",
        "pow": "pow_", "sqrt": "sqrt_",

        # Stack
        "dup": "dup_", "pop": "pop_", "swap": "swap_",
        "rollup": "rollup_", "rolldown": "rolldown_", "rotate": "rotate_",
        "dupd": "dupd_", "popd": "popd_", "swapd": "swapd_",
        "over": "over_", "pick": "pick_",
        "stack": "stack_", "unstack": "unstack_",

        # Comparison
        "<": "less_", ">": "greater_", "<=": "leql_", ">=": "geql_",
        "=": "eql_", "!=": "neql_",
        "compare": "compare_", "equal": "equal_",

        # Boolean
        "and": "and_", "or": "or_", "xor": "xor_", "not": "not_",
        "true": "true_", "false": "false_",

        # List/Aggregate
        "null": "null_", "size": "size_",
        "first": "first_", "rest": "rest_",
        "cons": "cons_", "swons": "swons_", "concat": "concat_",
        "reverse": "reverse_", "uncons": "uncons_", "unswons": "unswons_",
        "at": "at_", "of": "of_",
        "take": "take_", "drop": "drop_",
        "in": "in_", "has": "has_",

        # Combinators
        "i": "i_", "x": "x_", "dip": "dip_",
        "ifte": "ifte_", "branch": "branch_",
        "map": "map_", "filter": "filter_", "fold": "fold_",
        "step": "step_", "times": "times_",
        "while": "while_", "whiledo": "whiledo_",
        "primrec": "primrec_", "linrec": "linrec_", "binrec": "binrec_",
        "genrec": "genrec_", "tailrec": "tailrec_",
        "app1": "app1_", "app2": "app2_", "app3": "app3_",
        "cleave": "cleave_", "spread": "spread_",
        "infra": "infra_", "execu": "execu_",

        # Type checking
        "integer?": "integerQ_", "float?": "floatQ_",
        "char?": "charQ_", "string?": "stringQ_",
        "list?": "listQ_", "set?": "setQ_",
        "boolean?": "booleanQ_", "file?": "fileQ_",
        "leaf?": "leafQ_", "logical?": "logicalQ_",
        "user?": "userQ_",
        "integer": "integer_", "float": "float_", "char": "char_",

        # String
        "name": "name_", "body": "body_", "intern": "intern_",
        "strtol": "strtol_", "strtod": "strtod_",
        "format": "format_", "formatf": "formatf_",

        # I/O
        ".": "put_", "put": "put_", "putln": "putln_",
        "putch": "putch_", "putchars": "putchars_",
        "get": "get_", "getln": "getln_", "getch": "getch_",
        "include": "include_",
        "fopen": "fopen_", "fclose": "fclose_",
        "fread": "fread_", "fwrite": "fwrite_",
        "fflush": "fflush_", "ftell": "ftell_", "fseek": "fseek_",

        # System
        "time": "time_", "clock": "clock_",
        "localtime": "localtime_", "gmtime": "gmtime_", "mktime": "mktime_",
        "strftime": "strftime_",
        "getenv": "getenv_", "system": "system_",
        "argc": "argc_", "argv": "argv_",
        "quit": "quit_", "abort": "abort_",

        # Control
        "id": "id_", "newstack": "newstack_",
        "choice": "choice_", "opcase": "opcase_",

        # Set operations
        "union": "or_", "intersection": "and_", "difference": "diff_",
    }

    # Operations that can be inlined (no function call)
    INLINE_OPS = {
        # Stack ops
        "dup": """
    /* dup: X -> X X */
    { ONEPARAM("dup"); GNULLARY(env->stck); }""",

        "pop": """
    /* pop: X -> */
    { ONEPARAM("pop"); POP(env->stck); }""",

        "swap": """
    /* swap: X Y -> Y X */
    { TWOPARAMS("swap"); SAVESTACK; GBINARY(SAVED1); GNULLARY(SAVED2); POP(env->dump); }""",

        "rollup": """
    /* rollup: X Y Z -> Z X Y */
    { THREEPARAMS("rollup"); SAVESTACK;
      GTERNARY(SAVED1); GNULLARY(SAVED3); GNULLARY(SAVED2); POP(env->dump); }""",

        "rolldown": """
    /* rolldown: X Y Z -> Y Z X */
    { THREEPARAMS("rolldown"); SAVESTACK;
      GTERNARY(SAVED2); GNULLARY(SAVED1); GNULLARY(SAVED3); POP(env->dump); }""",

        # Arithmetic (integer fast path)
        "+": """
    /* +: I J -> K */
    if (nodetype(env->stck) == INTEGER_ && nodetype(nextnode1(env->stck)) == INTEGER_) {
        BINARY(INTEGER_NEWNODE, nodevalue(nextnode1(env->stck)).num + nodevalue(env->stck).num);
    } else { plus_(env); }""",

        "-": """
    /* -: I J -> K */
    if (nodetype(env->stck) == INTEGER_ && nodetype(nextnode1(env->stck)) == INTEGER_) {
        BINARY(INTEGER_NEWNODE, nodevalue(nextnode1(env->stck)).num - nodevalue(env->stck).num);
    } else { minus_(env); }""",

        "*": """
    /* *: I J -> K */
    if (nodetype(env->stck) == INTEGER_ && nodetype(nextnode1(env->stck)) == INTEGER_) {
        BINARY(INTEGER_NEWNODE, nodevalue(nextnode1(env->stck)).num * nodevalue(env->stck).num);
    } else { mul_(env); }""",

        "/": """
    /* /: I J -> K */
    if (nodetype(env->stck) == INTEGER_ && nodetype(nextnode1(env->stck)) == INTEGER_ && nodevalue(env->stck).num != 0) {
        BINARY(INTEGER_NEWNODE, nodevalue(nextnode1(env->stck)).num / nodevalue(env->stck).num);
    } else { divide_(env); }""",

        "pred": """
    /* pred: I -> J */
    if (nodetype(env->stck) == INTEGER_) {
        UNARY(INTEGER_NEWNODE, nodevalue(env->stck).num - 1);
    } else { pred_(env); }""",

        "succ": """
    /* succ: I -> J */
    if (nodetype(env->stck) == INTEGER_) {
        UNARY(INTEGER_NEWNODE, nodevalue(env->stck).num + 1);
    } else { succ_(env); }""",

        # Comparison
        "<": """
    /* <: I J -> B */
    if (nodetype(env->stck) == INTEGER_ && nodetype(nextnode1(env->stck)) == INTEGER_) {
        BINARY(BOOLEAN_NEWNODE, nodevalue(nextnode1(env->stck)).num < nodevalue(env->stck).num);
    } else { less_(env); }""",

        ">": """
    /* >: I J -> B */
    if (nodetype(env->stck) == INTEGER_ && nodetype(nextnode1(env->stck)) == INTEGER_) {
        BINARY(BOOLEAN_NEWNODE, nodevalue(nextnode1(env->stck)).num > nodevalue(env->stck).num);
    } else { greater_(env); }""",

        "<=": """
    /* <=: I J -> B */
    if (nodetype(env->stck) == INTEGER_ && nodetype(nextnode1(env->stck)) == INTEGER_) {
        BINARY(BOOLEAN_NEWNODE, nodevalue(nextnode1(env->stck)).num <= nodevalue(env->stck).num);
    } else { leql_(env); }""",

        ">=": """
    /* >=: I J -> B */
    if (nodetype(env->stck) == INTEGER_ && nodetype(nextnode1(env->stck)) == INTEGER_) {
        BINARY(BOOLEAN_NEWNODE, nodevalue(nextnode1(env->stck)).num >= nodevalue(env->stck).num);
    } else { geql_(env); }""",

        "=": """
    /* =: X Y -> B */
    if (nodetype(env->stck) == INTEGER_ && nodetype(nextnode1(env->stck)) == INTEGER_) {
        BINARY(BOOLEAN_NEWNODE, nodevalue(nextnode1(env->stck)).num == nodevalue(env->stck).num);
    } else { eql_(env); }""",

        "!=": """
    /* !=: X Y -> B */
    if (nodetype(env->stck) == INTEGER_ && nodetype(nextnode1(env->stck)) == INTEGER_) {
        BINARY(BOOLEAN_NEWNODE, nodevalue(nextnode1(env->stck)).num != nodevalue(env->stck).num);
    } else { neql_(env); }""",
    }

    # Inline versions without runtime checks (for --no-checks mode)
    INLINE_OPS_NOCHECK = {
        "dup": """
    /* dup (unchecked) */
    GNULLARY(env->stck);""",

        "pop": """
    /* pop (unchecked) */
    POP(env->stck);""",

        "swap": """
    /* swap (unchecked) */
    { SAVESTACK; GBINARY(SAVED1); GNULLARY(SAVED2); POP(env->dump); }""",

        "rollup": """
    /* rollup (unchecked) */
    { SAVESTACK; GTERNARY(SAVED1); GNULLARY(SAVED3); GNULLARY(SAVED2); POP(env->dump); }""",

        "rolldown": """
    /* rolldown (unchecked) */
    { SAVESTACK; GTERNARY(SAVED2); GNULLARY(SAVED1); GNULLARY(SAVED3); POP(env->dump); }""",

        "+": """
    /* + (unchecked integer) */
    BINARY(INTEGER_NEWNODE, nodevalue(nextnode1(env->stck)).num + nodevalue(env->stck).num);""",

        "-": """
    /* - (unchecked integer) */
    BINARY(INTEGER_NEWNODE, nodevalue(nextnode1(env->stck)).num - nodevalue(env->stck).num);""",

        "*": """
    /* * (unchecked integer) */
    BINARY(INTEGER_NEWNODE, nodevalue(nextnode1(env->stck)).num * nodevalue(env->stck).num);""",

        "/": """
    /* / (unchecked integer) */
    BINARY(INTEGER_NEWNODE, nodevalue(nextnode1(env->stck)).num / nodevalue(env->stck).num);""",

        "pred": """
    /* pred (unchecked integer) */
    UNARY(INTEGER_NEWNODE, nodevalue(env->stck).num - 1);""",

        "succ": """
    /* succ (unchecked integer) */
    UNARY(INTEGER_NEWNODE, nodevalue(env->stck).num + 1);""",

        "<": """
    /* < (unchecked integer) */
    BINARY(BOOLEAN_NEWNODE, nodevalue(nextnode1(env->stck)).num < nodevalue(env->stck).num);""",

        ">": """
    /* > (unchecked integer) */
    BINARY(BOOLEAN_NEWNODE, nodevalue(nextnode1(env->stck)).num > nodevalue(env->stck).num);""",

        "<=": """
    /* <= (unchecked integer) */
    BINARY(BOOLEAN_NEWNODE, nodevalue(nextnode1(env->stck)).num <= nodevalue(env->stck).num);""",

        ">=": """
    /* >= (unchecked integer) */
    BINARY(BOOLEAN_NEWNODE, nodevalue(nextnode1(env->stck)).num >= nodevalue(env->stck).num);""",

        "=": """
    /* = (unchecked integer) */
    BINARY(BOOLEAN_NEWNODE, nodevalue(nextnode1(env->stck)).num == nodevalue(env->stck).num);""",

        "!=": """
    /* != (unchecked integer) */
    BINARY(BOOLEAN_NEWNODE, nodevalue(nextnode1(env->stck)).num != nodevalue(env->stck).num);""",
    }

    def __init__(self, inline: bool = True, no_checks: bool = False, inline_loops: bool = None):
        self.inline = inline
        self.no_checks = no_checks
        # inline_loops defaults to same as inline if not specified
        self.inline_loops = inline_loops if inline_loops is not None else inline
        self.quotation_counter = 0
        self.quotations: list[tuple[str, QuotationNode]] = []
        self.definitions: dict[str, str] = {}  # Joy name -> C function name
        self.loop_counter = 0  # For unique loop variable names

    def emit(self, program: ProgramNode) -> str:
        """Generate C code for the program."""
        lines = []

        # Header
        lines.append(self._emit_header())

        # Collect all quotations and definitions
        self._collect_quotations(program)

        # Forward declarations
        lines.append("\n/* Forward declarations */")
        for name, c_name in self.definitions.items():
            lines.append(f"static void {c_name}(pEnv env);")
        for quot_name, _ in self.quotations:
            lines.append(f"static void {quot_name}(pEnv env);")
        lines.append("static void joy_main_(pEnv env);")

        # User-defined functions
        lines.append("\n/* User-defined functions */")
        for defn in program.definitions:
            lines.append(self._emit_definition(defn))

        # Quotation functions
        if self.quotations:
            lines.append("\n/* Quotation functions */")
            for quot_name, quot in self.quotations:
                lines.append(self._emit_quotation_function(quot_name, quot))

        # Main program
        lines.append("\n/* Main program */")
        lines.append(self._emit_main_function(program.main))

        # Entry point
        lines.append(self._emit_entry_point())

        return "\n".join(lines)

    def _emit_header(self) -> str:
        """Emit C file header."""
        return '''/**
 * Joy program compiled to C
 * Generated by joy2c.py
 *
 * Compile with:
 *   gcc -O3 -DNOBDW -I<joy>/include -I<joy>/build/generated -I<joy> program.c \\
 *       <joy>/build/libjoycore_static.a -lm -lsqlite3 -o program
 */

#include "globals.h"
#include "builtin.h"

/* Stack manipulation macros from runtime.h */
#define SAVESTACK env->dump = LIST_NEWNODE(env->stck, env->dump)
#define DMP nodevalue(env->dump).lis
#define SAVED1 DMP
#define SAVED2 nextnode1(DMP)
#define SAVED3 nextnode2(DMP)
#define SAVED4 nextnode3(DMP)
#define SAVED5 nextnode4(DMP)

/* Parameter validation macros (simplified - checks can be disabled with NCHECK) */
#ifndef NCHECK
#define ONEPARAM(NAME) if (!env->stck) { execerror(env, "one parameter", NAME); return; }
#define TWOPARAMS(NAME) if (!env->stck || !nextnode1(env->stck)) { execerror(env, "two parameters", NAME); return; }
#define THREEPARAMS(NAME) if (!env->stck || !nextnode1(env->stck) || !nextnode2(env->stck)) { execerror(env, "three parameters", NAME); return; }
#else
#define ONEPARAM(NAME)
#define TWOPARAMS(NAME)
#define THREEPARAMS(NAME)
#endif
'''

    def _collect_quotations(self, program: ProgramNode) -> None:
        """Collect all quotations and definitions for forward declarations."""
        for defn in program.definitions:
            c_name = self._sanitize_name(defn.name)
            self.definitions[defn.name] = c_name
            self._collect_quotations_in_quotation(defn.body)
        self._collect_quotations_in_quotation(program.main)

    def _collect_quotations_in_quotation(self, quot: QuotationNode) -> None:
        """Collect nested quotations, skipping those that will be inlined."""
        terms = quot.terms
        i = 0

        while i < len(terms):
            # Skip [cond] [body] while pattern - these will be inlined
            if (self.inline_loops and self.inline and
                i + 2 < len(terms) and
                isinstance(terms[i], QuotationNode) and
                isinstance(terms[i + 1], QuotationNode) and
                isinstance(terms[i + 2], SymbolNode) and
                terms[i + 2].name == "while"):
                # Still collect nested quotations within the inlined loop bodies
                self._collect_quotations_in_quotation(terms[i])
                self._collect_quotations_in_quotation(terms[i + 1])
                i += 3
                continue

            # Skip N [body] times pattern - these will be inlined
            if (self.inline_loops and self.inline and
                i + 2 < len(terms) and
                isinstance(terms[i], IntegerNode) and
                isinstance(terms[i + 1], QuotationNode) and
                isinstance(terms[i + 2], SymbolNode) and
                terms[i + 2].name == "times"):
                # Still collect nested quotations within the inlined loop body
                self._collect_quotations_in_quotation(terms[i + 1])
                i += 3
                continue

            # Regular quotation - collect it
            if isinstance(terms[i], QuotationNode):
                name = f"_quot_{self.quotation_counter}"
                self.quotation_counter += 1
                self.quotations.append((name, terms[i]))
                self._collect_quotations_in_quotation(terms[i])

            i += 1

    def _sanitize_name(self, name: str) -> str:
        """Convert Joy name to valid C identifier."""
        result = ["joy_word_"]
        for c in name:
            if c.isalnum():
                result.append(c)
            elif c == "-":
                result.append("_")
            elif c == "+":
                result.append("_plus")
            elif c == "*":
                result.append("_star")
            elif c == "/":
                result.append("_slash")
            elif c == "=":
                result.append("_eq")
            elif c == "<":
                result.append("_lt")
            elif c == ">":
                result.append("_gt")
            elif c == "?":
                result.append("Q")
            elif c == "!":
                result.append("_bang")
            else:
                result.append("_")
        return "".join(result)

    def _emit_definition(self, defn: DefinitionNode) -> str:
        """Emit a user-defined function."""
        c_name = self.definitions[defn.name]
        body = self._emit_quotation_body(defn.body, indent=1)
        return f"""
static void {c_name}(pEnv env)
{{
{body}
}}"""

    def _emit_quotation_function(self, name: str, quot: QuotationNode) -> str:
        """Emit a quotation as a callable function."""
        body = self._emit_quotation_body(quot, indent=1)
        return f"""
static void {name}(pEnv env)
{{
{body}
}}"""

    def _emit_quotation_body(self, quot: QuotationNode, indent: int = 0) -> str:
        """Emit the body of a quotation with loop inlining."""
        ind = "    " * indent
        lines = []
        terms = quot.terms
        i = 0

        while i < len(terms):
            # Check for [cond] [body] while pattern
            if (self.inline_loops and self.inline and
                i + 2 < len(terms) and
                isinstance(terms[i], QuotationNode) and
                isinstance(terms[i + 1], QuotationNode) and
                isinstance(terms[i + 2], SymbolNode) and
                terms[i + 2].name == "while"):
                code = self._emit_inline_while(terms[i], terms[i + 1], indent)
                if code:
                    lines.append(code)
                i += 3
                continue

            # Check for N [body] times pattern
            if (self.inline_loops and self.inline and
                i + 2 < len(terms) and
                isinstance(terms[i], IntegerNode) and
                isinstance(terms[i + 1], QuotationNode) and
                isinstance(terms[i + 2], SymbolNode) and
                terms[i + 2].name == "times"):
                code = self._emit_inline_times(terms[i].value, terms[i + 1], indent)
                if code:
                    lines.append(code)
                i += 3
                continue

            # Regular term
            code = self._emit_term(terms[i], indent)
            if code:
                lines.append(code)
            i += 1

        return "\n".join(lines) if lines else f"{ind}/* empty */"

    def _emit_inline_while(self, cond: QuotationNode, body: QuotationNode, indent: int) -> str:
        """Emit an inlined while loop using local variable for saved stack."""
        ind = "    " * indent
        self.loop_counter += 1
        loop_id = self.loop_counter

        lines = [f"{ind}/* inline while loop */"]
        lines.append(f"{ind}{{")
        lines.append(f"{ind}    Index _while_stck_{loop_id} = env->stck;")
        lines.append(f"{ind}    while (1) {{")
        lines.append(f"{ind}        env->stck = _while_stck_{loop_id};")

        # Emit condition body
        cond_code = self._emit_quotation_body(cond, indent + 2)
        lines.append(cond_code)

        # Check result and break if false
        lines.append(f"{ind}        {{")
        lines.append(f"{ind}            int _while_result_{loop_id} = nodevalue(env->stck).num;")
        lines.append(f"{ind}            POP(env->stck);")
        lines.append(f"{ind}            if (!_while_result_{loop_id}) break;")
        lines.append(f"{ind}        }}")

        # Restore stack and emit body
        lines.append(f"{ind}        env->stck = _while_stck_{loop_id};")

        body_code = self._emit_quotation_body(body, indent + 2)
        lines.append(body_code)

        # Save new stack state
        lines.append(f"{ind}        _while_stck_{loop_id} = env->stck;")
        lines.append(f"{ind}    }}")
        lines.append(f"{ind}    env->stck = _while_stck_{loop_id};")
        lines.append(f"{ind}}}")

        return "\n".join(lines)

    def _emit_inline_times(self, count: int, body: QuotationNode, indent: int) -> str:
        """Emit an inlined times loop."""
        ind = "    " * indent
        self.loop_counter += 1
        loop_id = self.loop_counter

        lines = [f"{ind}/* inline times loop ({count} iterations) */"]
        lines.append(f"{ind}{{")
        lines.append(f"{ind}    int64_t _times_i_{loop_id};")
        lines.append(f"{ind}    for (_times_i_{loop_id} = 0; _times_i_{loop_id} < {count}LL; _times_i_{loop_id}++) {{")

        # Emit body
        body_code = self._emit_quotation_body(body, indent + 2)
        lines.append(body_code)

        lines.append(f"{ind}    }}")
        lines.append(f"{ind}}}")

        return "\n".join(lines)

    def _emit_term(self, term: ASTNode, indent: int = 0) -> str:
        """Emit code for a single term."""
        ind = "    " * indent

        if isinstance(term, IntegerNode):
            return f"{ind}NULLARY(INTEGER_NEWNODE, {term.value}LL);"

        if isinstance(term, FloatNode):
            return f"{ind}NULLARY(FLOAT_NEWNODE, {term.value});"

        if isinstance(term, BooleanNode):
            val = 1 if term.value else 0
            return f"{ind}NULLARY(BOOLEAN_NEWNODE, {val});"

        if isinstance(term, CharNode):
            c = term.value
            if c == "\n":
                c_repr = "'\\n'"
            elif c == "\t":
                c_repr = "'\\t'"
            elif c == "\r":
                c_repr = "'\\r'"
            elif c == "\\":
                c_repr = "'\\\\'"
            elif c == "'":
                c_repr = "'\\''"
            else:
                c_repr = f"'{c}'"
            return f"{ind}NULLARY(CHAR_NEWNODE, {c_repr});"

        if isinstance(term, StringNode):
            escaped = (
                term.value
                .replace("\\", "\\\\")
                .replace('"', '\\"')
                .replace("\n", "\\n")
                .replace("\t", "\\t")
            )
            return f'{ind}NULLARY(STRING_NEWNODE, "{escaped}");'

        if isinstance(term, SetNode):
            if not term.members:
                return f"{ind}NULLARY(SET_NEWNODE, 0ULL);"
            bits = sum(1 << m for m in term.members)
            return f"{ind}NULLARY(SET_NEWNODE, {bits}ULL);"

        if isinstance(term, QuotationNode):
            # Find the quotation name
            for quot_name, quot in self.quotations:
                if quot is term:
                    # Push a quotation by creating a list node containing a function reference
                    # For now, generate inline code that builds the quotation at runtime
                    return self._emit_quotation_push(term, indent)
            # Fallback: emit inline
            return self._emit_quotation_push(term, indent)

        if isinstance(term, SymbolNode):
            return self._emit_symbol_call(term.name, indent)

        return f"{ind}/* unknown term type */"

    def _emit_quotation_push(self, quot: QuotationNode, indent: int) -> str:
        """Emit code to push a quotation onto the stack."""
        ind = "    " * indent

        if not quot.terms:
            return f"{ind}NULLARY(LIST_NEWNODE, 0);  /* empty quotation */"

        # Build quotation as a list in reverse order
        lines = [f"{ind}/* push quotation [{len(quot.terms)} terms] */"]
        lines.append(f"{ind}{{")
        lines.append(f"{ind}    Index _q = 0;")

        # Build in reverse (Joy lists are built by consing onto front)
        for term in reversed(quot.terms):
            if isinstance(term, IntegerNode):
                lines.append(f"{ind}    _q = INTEGER_NEWNODE({term.value}LL, _q);")
            elif isinstance(term, FloatNode):
                lines.append(f"{ind}    _q = FLOAT_NEWNODE({term.value}, _q);")
            elif isinstance(term, BooleanNode):
                val = 1 if term.value else 0
                lines.append(f"{ind}    _q = BOOLEAN_NEWNODE({val}, _q);")
            elif isinstance(term, CharNode):
                c = term.value
                if c == "\n":
                    c_repr = "'\\n'"
                elif c == "\t":
                    c_repr = "'\\t'"
                elif c == "'":
                    c_repr = "'\\''"
                elif c == "\\":
                    c_repr = "'\\\\'"
                else:
                    c_repr = f"'{c}'"
                lines.append(f"{ind}    _q = CHAR_NEWNODE({c_repr}, _q);")
            elif isinstance(term, StringNode):
                escaped = (
                    term.value
                    .replace("\\", "\\\\")
                    .replace('"', '\\"')
                    .replace("\n", "\\n")
                    .replace("\t", "\\t")
                )
                lines.append(f'{ind}    _q = STRING_NEWNODE("{escaped}", _q);')
            elif isinstance(term, SymbolNode):
                # Look up symbol in symtab and create USR node
                name = term.name
                if name in self.BUILTIN_MAP:
                    c_func = self.BUILTIN_MAP[name]
                    lines.append(f"{ind}    _q = ANON_FUNCT_NEWNODE({c_func}, _q);")
                elif name in self.definitions:
                    c_func = self.definitions[name]
                    lines.append(f"{ind}    _q = ANON_FUNCT_NEWNODE({c_func}, _q);")
                else:
                    # Unknown symbol - create USR node by looking up in symtab
                    lines.append(f'{ind}    _q = USR_NEWNODE(lookup(env, "{name}"), _q);')
            elif isinstance(term, QuotationNode):
                # Nested quotation - recursively build it
                # For simplicity, just note it as a TODO
                lines.append(f"{ind}    /* TODO: nested quotation */")
            elif isinstance(term, SetNode):
                bits = sum(1 << m for m in term.members) if term.members else 0
                lines.append(f"{ind}    _q = SET_NEWNODE({bits}ULL, _q);")
            else:
                lines.append(f"{ind}    /* unknown term in quotation */")

        lines.append(f"{ind}    NULLARY(LIST_NEWNODE, _q);")
        lines.append(f"{ind}}}")

        return "\n".join(lines)

    def _emit_symbol_call(self, name: str, indent: int) -> str:
        """Emit code to call a symbol."""
        ind = "    " * indent

        # Check for inline version
        if self.inline and name in (self.INLINE_OPS_NOCHECK if self.no_checks else self.INLINE_OPS):
            ops = self.INLINE_OPS_NOCHECK if self.no_checks else self.INLINE_OPS
            # Indent the inline code
            code = ops[name].strip()
            indented = "\n".join(ind + line if line.strip() else "" for line in code.split("\n"))
            return indented

        # Check for builtin
        if name in self.BUILTIN_MAP:
            c_func = self.BUILTIN_MAP[name]
            return f"{ind}{c_func}(env);"

        # Check for user-defined
        if name in self.definitions:
            c_func = self.definitions[name]
            return f"{ind}{c_func}(env);"

        # Unknown symbol - lookup at runtime
        return f'{ind}execsym(env, "{name}");  /* dynamic lookup */'

    def _emit_main_function(self, main: QuotationNode) -> str:
        """Emit the main program function."""
        body = self._emit_quotation_body(main, indent=1)
        return f"""
static void joy_main_(pEnv env)
{{
{body}
}}"""

    def _emit_entry_point(self) -> str:
        """Emit the C main() function."""
        return '''
/* Print remaining stack after program execution */
static void print_final_stack(pEnv env)
{
    if (env->stck) {
        Index p = env->stck;
        while (p) {
            writefactor(env, p, stdout);
            if (nextnode1(p)) printf(" ");
            POP(p);
        }
        printf("\\n");
    }
}

/* Entry point */
int main(int argc, char* argv[])
{
    Env _env;
    pEnv env = &_env;

    /* Initialize GC (required before any allocation) */
    GC_INIT();

    /* Initialize environment */
    memset(env, 0, sizeof(Env));

    /* Initialize vectors */
    vec_init(env->pathnames);
    vec_init(env->string);
    vec_init(env->pushback);
    vec_init(env->tokens);
    vec_init(env->symtab);

    /* Initialize symbol table with builtins */
    inisymboltable(env);

    /* Initialize memory (NOBDW mode) */
    inimem1(env, 0);
    inimem2(env);

    /* Default config */
    env->config.autoput = 1;
    env->config.undeferror = 1;

    /* Store command line args */
    env->g_argc = argc;
    env->g_argv = argv;

    /* Set up error handling */
    if (setjmp(env->error_jmp) == 0) {
        /* Run compiled program */
        joy_main_(env);

        /* Print final stack */
        print_final_stack(env);
    }

    return 0;
}
'''


# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Compile Joy programs to C code",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python tools/joy2c.py examples/fib.joy -o fib.c
  python tools/joy2c.py program.joy --inline --fold

The generated C code should be compiled with:
  gcc -O3 -DNOBDW -I<joy>/include output.c <joy>/build/libjoycore_static.a -lm
"""
    )
    parser.add_argument("input", help="Input Joy file")
    parser.add_argument("-o", "--output", help="Output C file (default: input.c)")
    parser.add_argument("--inline", action="store_true",
                        help="Inline simple operations")
    parser.add_argument("--fold", action="store_true",
                        help="Fold constant expressions")
    parser.add_argument("--no-checks", action="store_true",
                        help="Omit runtime parameter checks")
    parser.add_argument("--print", action="store_true",
                        help="Print generated code to stdout")

    args = parser.parse_args()

    # Read input
    input_path = Path(args.input)
    if not input_path.exists():
        print(f"Error: File not found: {args.input}", file=sys.stderr)
        sys.exit(1)

    source = input_path.read_text()

    # Determine output path
    if args.output:
        output_path = Path(args.output)
    else:
        output_path = input_path.with_suffix(".c")

    # Compile
    try:
        # Lex
        lexer = Lexer(source)
        tokens = lexer.tokenize()

        # Parse
        parser_obj = Parser(tokens)
        program = parser_obj.parse()

        # Optimize
        optimizer = Optimizer(fold_constants=args.fold, inline=args.inline)
        program = optimizer.optimize(program)

        # Emit
        emitter = CEmitter(inline=args.inline, no_checks=args.no_checks)
        c_code = emitter.emit(program)

        # Output
        if args.print:
            print(c_code)
        else:
            output_path.write_text(c_code)
            print(f"Generated: {output_path}")
            print(f"\nTo compile:")
            print(f"  gcc -O3 -DNOBDW -I<joy>/include {output_path} \\")
            print(f"      <joy>/build/libjoycore_static.a -lm -o {output_path.stem}")

    except SyntaxError as e:
        print(f"Syntax error: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        raise


if __name__ == "__main__":
    main()

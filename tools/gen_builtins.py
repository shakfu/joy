#!/usr/bin/env python3
"""
Generate builtin.c and builtin.h for the Joy interpreter.

The builtin directory structure:
  src/builtin/
    *.c           - Grouped builtin files (arithmetic.c, stack.c, etc.)
    *.h           - Shared macro headers

builtin.c includes the grouped .c files.
builtin.h declares all builtin functions extracted from the grouped files.
"""
import argparse
import pathlib
import re


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate builtin include glue")
    parser.add_argument("source_dir", type=pathlib.Path, help="Repository root")
    parser.add_argument(
        "output_dir", type=pathlib.Path, help="Directory for generated files"
    )
    return parser.parse_args()


def extract_function_names(source_file: pathlib.Path) -> tuple[list[str], set[str]]:
    """Extract builtin function names from a consolidated source file.

    Looks for:
    - void xxx_(pEnv env) - direct function definitions
    - MACRO(xxx_, ...) - macro invocations where first arg is function name

    Also identifies functions marked with [NATIVE] in their docstrings.

    Returns:
        (list of all function names, set of native-only function names)
    """
    content = source_file.read_text()
    names = set()
    native_names = set()

    # First, find all functions that have [NATIVE] in their preceding docstring
    # Pattern: /** ... [NATIVE] ... */ followed by void xxx_(pEnv env)
    doc_pattern = re.compile(
        r'/\*\*([^*]|\*(?!/))*\[NATIVE\]([^*]|\*(?!/))*\*/\s*void\s+(\w+_)\s*\(\s*pEnv\s+env\s*\)',
        re.DOTALL
    )
    for match in doc_pattern.finditer(content):
        func_name = match.group(3)
        native_names.add(func_name)

    # Pattern 1: void xxx_(pEnv env) function definitions
    for match in re.finditer(r'\bvoid\s+(\w+_)\s*\(\s*pEnv\s+env\s*\)', content):
        names.add(match.group(1))

    # Pattern 2: MACRO(xxx_, ...) - uppercase macro with function name as first arg
    # Matches things like PLUSMINUS(plus_, ...), UFLOAT(ceil_, ...), etc.
    for match in re.finditer(r'\b[A-Z][A-Z0-9_]+\s*\(\s*(\w+_)\s*,', content):
        names.add(match.group(1))

    return sorted(names), native_names


def emit_builtin_sources(source_dir: pathlib.Path, output_dir: pathlib.Path) -> None:
    prim_dir = source_dir / "src" / "builtin"
    output_dir.mkdir(parents=True, exist_ok=True)

    # Get grouped .c files (top-level only)
    grouped_sources = sorted(f for f in prim_dir.glob("*.c") if f.is_file())

    # Extract all function names from grouped files
    all_functions = set()
    all_native_functions = set()
    for src in grouped_sources:
        functions, native_functions = extract_function_names(src)
        all_functions.update(functions)
        all_native_functions.update(native_functions)

    # Generate builtin.c - includes grouped files
    builtin_c = output_dir / "builtin.c"
    with builtin_c.open("w") as bc_file:
        bc_file.write("/* Generated file - do not edit */\n")
        bc_file.write("/* Includes grouped builtin files */\n\n")
        for src in grouped_sources:
            rel = src.resolve().relative_to(source_dir.resolve())
            bc_file.write(f'#include "{rel.as_posix()}"\n')

    # Generate builtin.h - declares all functions
    # Separate regular and native-only functions
    regular_functions = sorted(all_functions - all_native_functions)
    native_functions = sorted(all_native_functions)

    builtin_h = output_dir / "builtin.h"
    with builtin_h.open("w") as bh_file:
        bh_file.write("#ifndef JOY_BUILTIN_GENERATED_H\n")
        bh_file.write("#define JOY_BUILTIN_GENERATED_H\n\n")
        bh_file.write("/* Generated file - do not edit */\n")
        bh_file.write("/* Declares all builtin functions */\n\n")
        bh_file.write('#include "globals.h"\n\n')
        # Regular functions
        for func_name in regular_functions:
            bh_file.write(f"void {func_name}(pEnv env);\n")
        # Native-only functions
        if native_functions:
            bh_file.write("\n#ifdef JOY_NATIVE_TYPES\n")
            for func_name in native_functions:
                bh_file.write(f"void {func_name}(pEnv env);\n")
            bh_file.write("#endif /* JOY_NATIVE_TYPES */\n")
        bh_file.write("\n#endif /* JOY_BUILTIN_GENERATED_H */\n")


def main() -> None:
    args = parse_args()
    emit_builtin_sources(args.source_dir, args.output_dir)


if __name__ == "__main__":
    main()

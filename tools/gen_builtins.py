#!/usr/bin/env python3
"""
Generate builtin.c and builtin.h for the Joy interpreter.

The builtin directory structure:
  src/builtin/
    *.c           - Grouped builtin files (arithmetic.c, stack.c, etc.)
    *.h           - Shared macro headers
    individual/   - Individual builtin implementations (one per file)

builtin.c includes the grouped .c files which in turn include individual files.
builtin.h declares all individual builtin functions for the optable.
"""
import argparse
import pathlib


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate builtin include glue")
    parser.add_argument("source_dir", type=pathlib.Path, help="Repository root")
    parser.add_argument(
        "output_dir", type=pathlib.Path, help="Directory for generated files"
    )
    return parser.parse_args()


def emit_builtin_sources(source_dir: pathlib.Path, output_dir: pathlib.Path) -> None:
    prim_dir = source_dir / "src" / "builtin"
    individual_dir = prim_dir / "individual"
    output_dir.mkdir(parents=True, exist_ok=True)

    # Get grouped .c files (top-level only, not individual/)
    grouped_sources = sorted(f for f in prim_dir.glob("*.c") if f.is_file())

    # Get individual .c files for function declarations
    individual_sources = sorted(individual_dir.glob("*.c"))

    # Generate builtin.c - includes grouped files
    builtin_c = output_dir / "builtin.c"
    with builtin_c.open("w") as bc_file:
        bc_file.write("/* Generated file - do not edit */\n")
        bc_file.write("/* Includes grouped builtin files */\n\n")
        for src in grouped_sources:
            rel = src.resolve().relative_to(source_dir.resolve())
            bc_file.write(f'#include "{rel.as_posix()}"\n')

    # Generate builtin.h - declares all individual functions
    builtin_h = output_dir / "builtin.h"
    with builtin_h.open("w") as bh_file:
        bh_file.write("#ifndef JOY_BUILTIN_GENERATED_H\n")
        bh_file.write("#define JOY_BUILTIN_GENERATED_H\n\n")
        bh_file.write("/* Generated file - do not edit */\n")
        bh_file.write("/* Declares all builtin functions */\n\n")
        bh_file.write('#include "globals.h"\n\n')
        for src in individual_sources:
            bh_file.write(f"void {src.stem}_(pEnv env);\n")
        bh_file.write("\n#endif /* JOY_BUILTIN_GENERATED_H */\n")


def main() -> None:
    args = parse_args()
    emit_builtin_sources(args.source_dir, args.output_dir)


if __name__ == "__main__":
    main()

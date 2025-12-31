#!/usr/bin/env python3
"""
Generate builtin.c and builtin.h for the Joy interpreter.
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
    sources = sorted(prim_dir.glob("*.c"))
    output_dir.mkdir(parents=True, exist_ok=True)

    builtin_c = output_dir / "builtin.c"
    with builtin_c.open("w") as bc_file:
        for src in sources:
            bc_file.write(f'#include "{src.resolve().as_posix()}"\n')

    builtin_h = output_dir / "builtin.h"
    with builtin_h.open("w") as bh_file:
        for src in sources:
            bh_file.write(f"void {src.stem}_(pEnv env);\n")


def main() -> None:
    args = parse_args()
    emit_builtin_sources(args.source_dir, args.output_dir)


if __name__ == "__main__":
    main()

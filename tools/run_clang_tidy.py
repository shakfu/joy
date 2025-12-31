#!/usr/bin/env python3
"""
Run clang-tidy across all sources that appear in compile_commands.json.
"""
import json
import os
import pathlib
import subprocess
import sys
from typing import List


def load_sources(build_dir: pathlib.Path) -> List[str]:
    database = build_dir / "compile_commands.json"
    try:
        entries = json.loads(database.read_text())
    except FileNotFoundError as exc:
        raise SystemExit(f"{database} not found; run `make joy` first.") from exc

    sources: List[str] = []
    seen = set()
    src_root = (build_dir.parent / "src").resolve()
    for entry in entries:
        path = pathlib.Path(entry["file"]).resolve()
        try:
            path.relative_to(src_root)
        except ValueError:
            continue
        if path in seen:
            continue
        seen.add(path)
        sources.append(str(path))
    if not sources:
        raise SystemExit("No source files discovered for clang-tidy.")
    return sources


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: run_clang_tidy.py <build-dir> <clang-tidy-path>")
    build_dir = pathlib.Path(sys.argv[1]).resolve()
    clang_tidy = sys.argv[2]
    sources = load_sources(build_dir)
    env = os.environ.copy()
    if "SDKROOT" not in env:
        try:
            sdkroot = (
                subprocess.check_output(["xcrun", "--show-sdk-path"])
                .decode()
                .strip()
            )
            env["SDKROOT"] = sdkroot
        except Exception:
            pass
    for source in sources:
        print(f"Running clang-tidy on {source}")
        result = subprocess.run(
            [
                clang_tidy,
                "-p",
                str(build_dir),
                "--header-filter=src/.*",
                source,
            ],
            check=False,
            env=env,
        )
        if result.returncode:
            raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()

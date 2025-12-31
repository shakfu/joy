#!/usr/bin/env python3
import argparse
import pathlib
import re

def parse_args():
    parser = argparse.ArgumentParser(description="Generate table.c from primitive docs")
    parser.add_argument("source_dir", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    return parser.parse_args()

def escape(value: str) -> str:
    return value.replace("\"", "\\\"").replace("\n", "\\n")

def main():
    args = parse_args()
    prim_dir = args.source_dir / "src" / "builtin"
    entries = []
    comment_re = re.compile(r"/\*\*(.*?)\*/", re.S)
    for c_path in sorted(prim_dir.glob("*.c")):
        text = c_path.read_text()
        match = comment_re.search(text)
        if not match:
            continue
        block = match.group(1)
        lines = [line.rstrip() for line in block.splitlines()]
        while lines and not lines[0].strip():
            lines.pop(0)
        if not lines:
            continue
        header = lines[0]
        if not header.startswith("Q"):
            continue
        header_parts = header.split(":", 1)
        lhs = header_parts[0].strip()
        signature = header_parts[1].strip() if len(header_parts) > 1 else ""
        lhs_tokens = lhs.split()
        if len(lhs_tokens) < 4:
            continue
        qcode, flags, index, name = lhs_tokens[:4]
        desc = "\n".join(lines[1:]).strip()
        try:
            order = int(index)
        except ValueError:
            continue
        entries.append({
            "index": order,
            "index_str": index,
            "qcode": qcode,
            "flags": flags,
            "name": name,
            "proc": f"{c_path.stem}_",
            "signature": signature,
            "desc": desc,
        })
    entries.sort(key=lambda item: item["index"])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w") as outfile:
        for entry in entries:
            line = (
                f"/* {entry['index_str']:>4} */ {{ {entry['qcode']}, {entry['flags']}, "
                f"\"{escape(entry['name'])}\", {entry['proc']}, "
                f"\"{escape(entry['signature'])}\", \"{escape(entry['desc'])}\" }},\n"
            )
            outfile.write(line)

if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
Generate markdown documentation from Joy library files.

Extracts (* ... *) comments and definitions from .joy files,
organizing them into readable markdown documentation.

Usage:
    python tools/gen_docs.py lib/*.joy > docs/library.md
    python tools/gen_docs.py lib/numlib.joy  # single file
"""
import argparse
import pathlib
import re
import sys
from dataclasses import dataclass, field


@dataclass
class Definition:
    """A Joy definition with its documentation."""
    name: str
    body: str
    comment: str = ""
    inline_comment: str = ""


@dataclass
class Section:
    """A section of definitions, optionally with a header."""
    header: str = ""
    definitions: list = field(default_factory=list)


@dataclass
class JoyFile:
    """Parsed Joy file with sections and definitions."""
    filename: str
    file_comment: str = ""
    sections: list = field(default_factory=list)


def parse_joy_file(path: pathlib.Path) -> JoyFile:
    """Parse a Joy file and extract documentation."""
    content = path.read_text()
    result = JoyFile(filename=path.name)

    # Extract file header comment
    file_match = re.match(r'\(\*\s*FILE:\s*(\S+)\s*\*\)', content)
    if file_match:
        result.file_comment = f"FILE: {file_match.group(1)}"

    # Section headers look like: (* - - - - -  SECTION NAME  - - - - - *)
    # or simpler: (* section name *)
    section_pattern = re.compile(r'\(\*\s*-[\s-]*([A-Z][A-Z\s]+[A-Z])[\s-]*-\s*\*\)')

    # Simple section headers: (* predicates *) or (* functions *)
    simple_section = re.compile(r'^\(\*\s*([a-z][a-z\s]+[a-z])\s*\*\)\s*$', re.MULTILINE)

    # Find all section headers with their positions
    section_markers = []
    for m in section_pattern.finditer(content):
        # Clean up spaced-out headers like "O P E R A T O R S"
        name = m.group(1).strip()
        if ' ' in name and all(len(w) <= 2 for w in name.split()):
            name = name.replace(' ', '')
        section_markers.append((m.start(), name.title()))
    for m in simple_section.finditer(content):
        section_markers.append((m.start(), m.group(1).strip().title()))
    section_markers.sort(key=lambda x: x[0])

    # Match definitions: name == body ;
    define_pattern = re.compile(
        r'^\s*(?:DEFINE\s+)?(\w[\w-]*)\s*==\s*'  # Definition name
        r'((?:[^;]|;(?!\s*$))*?)'  # Body (non-greedy, stop at line-ending ;)
        r'\s*;'  # Terminating semicolon
        r'[ \t]*(?:\(\*([^*\n]*)\*\))?',  # Optional inline comment (same line)
        re.MULTILINE
    )

    # Build a map of definition positions
    definitions = []
    for match in define_pattern.finditer(content):
        name = match.group(1)
        body = match.group(2).strip()
        inline = (match.group(3) or "").strip()
        pos = match.start()

        # Skip internal markers like _libname == true
        if name.startswith('_'):
            continue

        # Look for a preceding comment (within 200 chars before, not a section header)
        # Must be a standalone comment block just before this definition
        before = content[max(0, pos-300):pos]
        preceding = ""
        comment_match = re.search(
            r'\(\*\s*((?:[^*]|\*(?!\)))+?)\s*\*\)\s*$',
            before
        )
        if comment_match:
            candidate = comment_match.group(1).strip()
            # Skip if it looks like a section header or file header
            if not re.match(r'^-[\s-]*[A-Z]', candidate) and 'FILE:' not in candidate:
                # Skip if it looks like commented-out code
                if '==' not in candidate and not candidate.startswith('['):
                    # Skip if it's just a single word (likely section header)
                    if ' ' in candidate or len(candidate) > 20:
                        preceding = candidate

        definitions.append((pos, Definition(
            name=name,
            body=body,
            comment=preceding,
            inline_comment=inline
        )))

    # Organize definitions into sections
    current_section = Section(header="Definitions")
    result.sections.append(current_section)
    current_section_start = 0

    for defn_pos, defn in definitions:
        # Check if we've crossed into a new section
        for sect_pos, sect_name in section_markers:
            if sect_pos > current_section_start and sect_pos < defn_pos:
                if sect_name != current_section.header:
                    current_section = Section(header=sect_name)
                    result.sections.append(current_section)
                    current_section_start = sect_pos

        current_section.definitions.append(defn)

    # Remove empty sections
    result.sections = [s for s in result.sections if s.definitions]

    return result


def format_markdown(files: list[JoyFile], title: str = "Joy Library Reference") -> str:
    """Format parsed files as markdown."""
    lines = []
    lines.append(f"# {title}\n")
    lines.append("Auto-generated documentation from Joy library files.\n")

    # Table of contents
    lines.append("## Contents\n")
    for jf in files:
        anchor = jf.filename.replace('.', '').lower()
        lines.append(f"- [{jf.filename}](#{anchor})")
    lines.append("")

    # Each file
    for jf in files:
        anchor = jf.filename.replace('.', '').lower()
        lines.append(f"## {jf.filename}\n")

        for section in jf.sections:
            if section.header:
                lines.append(f"### {section.header.title()}\n")

            for defn in section.definitions:
                # Definition header
                lines.append(f"#### `{defn.name}`\n")

                # Body as code block
                body_display = defn.body
                if len(body_display) > 60:
                    # Multi-line display for long definitions
                    lines.append("```joy")
                    lines.append(f"{defn.name} ==")
                    lines.append(f"    {body_display}")
                    lines.append("```\n")
                else:
                    lines.append(f"```joy\n{defn.name} == {body_display}\n```\n")

                # Documentation
                if defn.comment:
                    # Clean up the comment
                    comment = defn.comment.replace('\n', ' ').strip()
                    lines.append(f"{comment}\n")
                elif defn.inline_comment:
                    lines.append(f"{defn.inline_comment}\n")

                lines.append("")

        lines.append("---\n")

    return '\n'.join(lines)


def format_simple(files: list[JoyFile]) -> str:
    """Format as a simple definition list."""
    lines = []
    for jf in files:
        lines.append(f"=== {jf.filename} ===\n")
        for section in jf.sections:
            if section.header:
                lines.append(f"--- {section.header} ---\n")
            for defn in section.definitions:
                doc = defn.comment or defn.inline_comment or ""
                if doc:
                    lines.append(f"{defn.name}  :  {doc}")
                else:
                    lines.append(f"{defn.name}  ==  {defn.body[:50]}...")
        lines.append("")
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Generate documentation from Joy library files"
    )
    parser.add_argument(
        "files",
        nargs="+",
        type=pathlib.Path,
        help="Joy library files to document"
    )
    parser.add_argument(
        "-o", "--output",
        type=pathlib.Path,
        help="Output file (default: stdout)"
    )
    parser.add_argument(
        "-t", "--title",
        default="Joy Library Reference",
        help="Document title"
    )
    parser.add_argument(
        "--simple",
        action="store_true",
        help="Simple output format (not markdown)"
    )
    args = parser.parse_args()

    # Parse all files
    parsed = []
    for path in args.files:
        if path.exists() and path.suffix == '.joy':
            try:
                parsed.append(parse_joy_file(path))
            except Exception as e:
                print(f"Warning: Failed to parse {path}: {e}", file=sys.stderr)

    if not parsed:
        print("No Joy files found or parsed.", file=sys.stderr)
        return 1

    # Generate output
    if args.simple:
        output = format_simple(parsed)
    else:
        output = format_markdown(parsed, title=args.title)

    # Write output
    if args.output:
        args.output.write_text(output)
        print(f"Wrote documentation to {args.output}", file=sys.stderr)
    else:
        print(output)

    return 0


if __name__ == "__main__":
    sys.exit(main())

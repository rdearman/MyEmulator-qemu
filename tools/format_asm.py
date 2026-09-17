#!/usr/bin/env python3
"""Small formatter for MyEmulator assembly source files.

The formatter is deliberately conservative: it changes indentation and
trailing whitespace, but preserves assembler spelling, strings, immediates,
and MyEmulator's semicolon comments.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re


LABEL_RE = re.compile(r"^((?:[0-9]+|[A-Za-z_.$][\w.$]*):)(?:\s*(.*))?$")


def split_comment(line: str) -> tuple[str, str]:
    """Split a line at a semicolon outside a double-quoted string."""
    quoted = False
    escaped = False
    for index, char in enumerate(line):
        if quoted:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quoted = False
        elif char == '"':
            quoted = True
        elif char == ';':
            return line[:index].rstrip(), line[index:].strip()
    return line.rstrip(), ""


def format_code(code: str) -> str:
    """Format one comment-free source line."""
    code = code.strip()
    if not code:
        return ""

    # Preserve labels at the left margin, including `label: instruction`.
    label = LABEL_RE.match(code)
    if label:
        name, remainder = label.group(1), (label.group(2) or "").strip()
        if not remainder:
            return name
        if remainder.startswith('.'):
            return f"{name} {remainder}"
        return f"{name}\n\t{remainder}"

    # Directives are conventionally left-aligned; instructions are indented.
    return code if code.startswith('.') else f"\t{code}"


def format_source(text: str) -> str:
    """Return formatted MyEmulator assembly, always ending in one newline."""
    output = []
    for raw_line in text.splitlines():
        stripped = raw_line.strip()
        if not stripped:
            output.append("")
            continue
        if stripped.startswith(';'):
            output.append(stripped)
            continue

        code, comment = split_comment(raw_line)
        formatted = format_code(code)
        if comment:
            # A label followed by an instruction becomes two physical lines;
            # put the comment on the instruction line in that case.
            if "\n" in formatted:
                lines = formatted.splitlines()
                lines[-1] += f"    {comment}"
                formatted = "\n".join(lines)
            elif formatted:
                formatted += f"    {comment}"
            else:
                formatted = comment
        output.extend(formatted.splitlines() or [""])
    return "\n".join(output).rstrip("\n") + "\n"


def format_file(input_file: str | Path, output_file: str | Path) -> None:
    source = Path(input_file)
    destination = Path(output_file)
    destination.write_text(format_source(source.read_text()))


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="Format MyEmulator assembly source")
    parser.add_argument("input_file", help="input .s file")
    parser.add_argument("output_file", help="formatted output .s file")
    args = parser.parse_args(argv)
    try:
        format_file(args.input_file, args.output_file)
    except OSError as exc:
        parser.error(str(exc))
    print(f"Formatted assembly code saved to {args.output_file}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

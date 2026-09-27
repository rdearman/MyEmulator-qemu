#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
docs="$root/docs"
src="$docs/Inside_REM_Edition2.md"
tmp=$(mktemp --suffix=.md)
trap 'rm -f "$tmp"' EXIT

# The Markdown title block is supplied as Pandoc metadata so the first page
# is a proper title page rather than an ordinary chapter.
python3 - "$src" "$tmp" <<'PY'
from pathlib import Path
import sys

src, dst = map(Path, sys.argv[1:])
text = src.read_text()
marker = "\n---\n"
if not text.startswith("# Inside the REM Computer") or marker not in text:
    raise SystemExit("unexpected Edition 2 title block")
body = text.split(marker, 1)[1].lstrip("\n")
dst.write_text(body)
PY

pandoc "$tmp" \
  --from=markdown+pipe_tables+fenced_code_blocks+raw_attribute \
  --to=latex --standalone --top-level-division=chapter \
  -V documentclass=book -V classoption=oneside -V classoption=openany \
  --toc --toc-depth=1 \
  --metadata=title:'Inside the REM Computer' \
  --metadata=subtitle:'Edition 2 — Expanded Offline Reference' \
  --metadata=author:'REM Project' \
  --lua-filter="$docs/rem-book.lua" \
  --include-in-header="$docs/rem-book-header.tex" \
  --highlight-style=tango \
  -o "$docs/Inside_REM_Edition2.tex"

# A few legacy source blocks contain a fence marker that Pandoc preserves
# inside an otherwise valid verbatim block.  It is a formatting marker, not
# part of the example, so remove it from the generated TeX before typesetting.
sed -i -E '/^~~~(asm|sh)$/d' "$docs/Inside_REM_Edition2.tex"
# A number of the older diagram blocks use an empty tilde fence as a visual
# delimiter. Pandoc can preserve that delimiter inside a verbatim paragraph
# when adjacent blocks have no separating prose. It is a Markdown marker, not
# book content; remove only the literal three-tilde token from generated TeX.
sed -i 's/\\textasciitilde\\textasciitilde\\textasciitilde{}//g' "$docs/Inside_REM_Edition2.tex"

xelatex -interaction=nonstopmode -halt-on-error \
  -output-directory="$docs" "$docs/Inside_REM_Edition2.tex" >/tmp/rem-book-xelatex-1.log
xelatex -interaction=nonstopmode -halt-on-error \
  -output-directory="$docs" "$docs/Inside_REM_Edition2.tex" >/tmp/rem-book-xelatex-2.log

rm -f "$docs/Inside_REM_Edition2.aux" "$docs/Inside_REM_Edition2.log" \
  "$docs/Inside_REM_Edition2.toc" "$docs/Inside_REM_Edition2.out"

if command -v pdftotext >/dev/null 2>&1; then
  if pdftotext "$docs/Inside_REM_Edition2.pdf" - | grep -Eq '\{=latex\}|\\REM(SoftwareStack|Flow|Horizontal)'; then
    echo "raw LaTeX markup leaked into the PDF" >&2
    exit 1
  fi
fi

echo "Wrote $docs/Inside_REM_Edition2.pdf"

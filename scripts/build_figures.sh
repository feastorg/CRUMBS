#!/usr/bin/env bash
# Builds every docs/figures/src/*.tex into docs/figures/<name>.svg:
# pdflatex, then pdftocairo -svg. Needs a TeX Live with tikz and
# poppler-utils. Build intermediates go to a temporary directory.
#
#   scripts/build_figures.sh            build every figure
#   scripts/build_figures.sh frame      build docs/figures/src/frame.tex only
set -euo pipefail
cd "$(dirname "$0")/.."

src=docs/figures/src
out=docs/figures
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

if [ $# -gt 0 ]; then
    names=("$@")
else
    names=()
    for f in "$src"/*.tex; do
        names+=("$(basename "$f" .tex)")
    done
fi

for name in "${names[@]}"; do
    if ! TEXINPUTS="$src:" pdflatex -interaction=nonstopmode -halt-on-error \
        -output-directory "$tmp" "$src/$name.tex" >"$tmp/$name.stdout"; then
        cat "$tmp/$name.stdout" >&2
        echo "build_figures: $name.tex failed" >&2
        exit 1
    fi
    pdftocairo -svg "$tmp/$name.pdf" "$tmp/$name.svg"
    # pdftocairo gives the root a unitless width and height equal to the
    # size in points, so a browser shows 1 pt as 1 px and the labels come
    # out at 7-9 px. Double the declared size; the viewBox keeps the
    # drawing, and a page narrower than the figure scales it down.
    python3 -I - "$tmp/$name.svg" "$out/$name.svg" <<'EOF'
import re
import sys

src, dst = sys.argv[1], sys.argv[2]
svg = open(src, encoding="utf-8").read()


def double(m):
    return f'{m.group(1)}="{float(m.group(2)) * 2:g}"'


root = re.search(r"<svg\b[^>]*>", svg)
new_root, n = re.subn(r'\b(width|height)="([0-9.]+)"', double, root.group(0))
if n != 2:
    sys.exit(f"build_figures: expected a unitless width and height on <svg> in {src}")
open(dst, "w", encoding="utf-8").write(svg[: root.start()] + new_root + svg[root.end() :])
EOF
    echo "build_figures: $out/$name.svg"
done

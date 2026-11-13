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
    # The SVG's viewBox is the figure in points. pdftocairo versions write
    # the root's width and height differently (poppler 25.03 writes unitless
    # points, which a browser draws as 1 px per pt, with labels at 7-9 px).
    # Set both to twice the viewBox, in px, so every poppler gives the same
    # size; a page narrower than the figure scales it down.
    python3 -I - "$tmp/$name.svg" "$out/$name.svg" <<'EOF'
import re
import sys

src, dst = sys.argv[1], sys.argv[2]
svg = open(src, encoding="utf-8").read()
root = re.search(r"<svg\b[^>]*>", svg)
box = re.search(r'\bviewBox="[-0-9.]+ [-0-9.]+ ([0-9.]+) ([0-9.]+)"', root.group(0))
if box is None:
    sys.exit(f"build_figures: no viewBox in {src}: {root.group(0)}")
size = {"width": float(box.group(1)) * 2, "height": float(box.group(2)) * 2}
new_root, n = re.subn(
    r'\b(width|height)="[^"]*"', lambda m: f'{m.group(1)}="{size[m.group(1)]:g}"', root.group(0)
)
if n != 2:
    sys.exit(f"build_figures: no width and height in {src}: {root.group(0)}")
open(dst, "w", encoding="utf-8").write(svg[: root.start()] + new_root + svg[root.end() :])
EOF
    echo "build_figures: $out/$name.svg"
done

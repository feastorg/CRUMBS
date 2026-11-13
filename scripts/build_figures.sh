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
    pdftocairo -svg "$tmp/$name.pdf" "$out/$name.svg"
    echo "build_figures: $out/$name.svg"
done

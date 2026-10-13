#!/usr/bin/env bash
# Packs the PlatformIO library from a clean export of a git ref and fails
# unless the tarball holds exactly the files meant to ship.
#
#   scripts/check_pio_package.sh <output dir> [ref]    (ref defaults to HEAD)
#
# The export comes from `git archive`, so nothing untracked or gitignored in
# the working tree reaches the package. The expected list is built here from
# git, not read from library.json, so an edit to the manifest's "export"
# field that changes what users download fails until this list changes too.
# The checked tarball is <output dir>/CRUMBS.tar.gz; publish that file.
set -euo pipefail

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "usage: $0 <output dir> [ref]" >&2
    exit 64
fi
out=$1
ref=${2:-HEAD}
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)

tree=$(mktemp -d)
trap 'rm -rf "$tree"' EXIT
git -C "$root" archive "$ref" | tar -x -C "$tree"

mkdir -p "$out"
tarball="$out/CRUMBS.tar.gz"
pio pkg pack "$tree" --output "$tarball" >/dev/null

{
    git -C "$root" ls-tree -r --name-only "$ref" -- src examples
    printf '%s\n' CHANGELOG.md LICENSE README.md library.json library.properties
} | LC_ALL=C sort >"$out/expected.txt"
tar -tzf "$tarball" | LC_ALL=C sort >"$out/packed.txt"

if ! diff -u "$out/expected.txt" "$out/packed.txt"; then
    echo "error: $tarball does not hold the expected files (- missing, + extra)" >&2
    exit 1
fi
echo "$tarball: $(wc -l <"$out/packed.txt") files, as expected"

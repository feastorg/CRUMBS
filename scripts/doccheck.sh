#!/usr/bin/env bash
# Fails if any symbol in a public header lacks a Doxygen comment
# (WARN_AS_ERROR = FAIL_ON_WARNINGS in docs/Doxyfile). Doxygen only checks
# file-scope members of headers that carry a @file block, so require one.
set -euo pipefail
cd "$(dirname "$0")/.."
missing=$(grep -L '@file' src/*.h || true)
if [ -n "$missing" ]; then
    echo "headers without a @file block (their functions and macros would go unchecked):" >&2
    echo "$missing" >&2
    exit 1
fi
doxygen docs/Doxyfile
echo "doxygen: public headers fully documented, no warnings"

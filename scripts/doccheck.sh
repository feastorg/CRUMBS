#!/usr/bin/env bash
# Fails if any symbol in a public header lacks a Doxygen comment
# (WARN_AS_ERROR = FAIL_ON_WARNINGS in docs/Doxyfile).
set -euo pipefail
cd "$(dirname "$0")/.."
doxygen docs/Doxyfile
echo "doxygen: public headers fully documented, no warnings"

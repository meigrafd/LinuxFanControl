#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$ROOT/build"
TYPE="${1:-RelWithDebInfo}"
echo "[i] Build type      : $TYPE"
echo "[i] Root            : $ROOT"
echo "[i] Build dir       : $BUILD"
mkdir -p "$BUILD"
cd "$BUILD"
cmake -DCMAKE_BUILD_TYPE="$TYPE" ..
cmake --build . -j
echo
echo "[i] Binaries:"
[[ -f ./lfcd ]] && echo "  - $(realpath ./lfcd) (daemon)"

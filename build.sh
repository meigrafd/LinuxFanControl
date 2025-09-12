#!/usr/bin/env bash
set -euo pipefail

# Simple top-level build script for daemon + gui
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT}/build"
TYPE="${1:-RelWithDebInfo}"

echo "[i] Build type: $TYPE"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -DCMAKE_BUILD_TYPE="$TYPE" ..
cmake --build . -j

echo
echo "[i] Binaries:"
[[ -f ./lfcd ]] && echo "  - $(realpath ./lfcd)      (daemon)"
[[ -f ./lfc-gui ]] && echo "  - $(realpath ./lfc-gui)  (gui)"

#!/usr/bin/env bash
set -euo pipefail

# ---------
# Settings
# ---------
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT}/build"
TYPE="${1:-RelWithDebInfo}"          # usage: ./build.sh [Debug|RelWithDebInfo|Release]
FRESH="${FRESH:-0}"                  # set FRESH=1 ./build.sh  -> rm -rf build
GENERATOR="${GENERATOR:-}"           # e.g. GENERATOR="Ninja"

# Allow overrides from env; otherwise use common defaults
SENSORS_INCLUDE_DIRS="${SENSORS_INCLUDE_DIRS:-/usr/include}"
# Try common lib paths
if [[ -z "${SENSORS_LIBRARIES:-}" ]]; then
  for cand in \
    /usr/lib64/libsensors.so \
    /usr/lib/x86_64-linux-gnu/libsensors.so \
    /usr/local/lib64/libsensors.so \
    /usr/local/lib/libsensors.so
  do
    [[ -f "$cand" ]] && SENSORS_LIBRARIES="$cand" && break
  done
fi
SENSORS_LIBRARIES="${SENSORS_LIBRARIES:-/usr/lib64/libsensors.so}"

# -------------
# Sanity checks
# -------------
echo "[i] Build type      : $TYPE"
echo "[i] Root            : $ROOT"
echo "[i] Build dir       : $BUILD_DIR"
echo "[i] sensors.h       : ${SENSORS_INCLUDE_DIRS}/sensors/sensors.h"
echo "[i] libsensors.so   : ${SENSORS_LIBRARIES}"

if [[ ! -f "${SENSORS_INCLUDE_DIRS}/sensors/sensors.h" ]]; then
  echo "[!] sensors.h not found in ${SENSORS_INCLUDE_DIRS}/sensors"
  echo "    Set SENSORS_INCLUDE_DIRS or install lm_sensors-devel."
  exit 1
fi
if [[ ! -f "${SENSORS_LIBRARIES}" ]]; then
  echo "[!] libsensors not found at ${SENSORS_LIBRARIES}"
  echo "    Set SENSORS_LIBRARIES to the full path of libsensors.so."
  exit 1
fi

# --------------
# Prepare build
# --------------
if [[ "$FRESH" == "1" ]]; then
  echo "[i] Fresh build requested -> removing ${BUILD_DIR}"
  rm -rf "${BUILD_DIR}"
fi

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# If cache exists but points to a different source dir, nuke it to avoid CMake source-mismatch
if [[ -f CMakeCache.txt ]]; then
  SRC_IN_CACHE="$(grep -E '^CMAKE_HOME_DIRECTORY:' CMakeCache.txt | cut -d= -f2 || true)"
  if [[ -n "${SRC_IN_CACHE}" && "${SRC_IN_CACHE}" != "${ROOT}" ]]; then
    echo "[i] CMake cache points to different source (${SRC_IN_CACHE}), clearing build dir…"
    cd "${ROOT}"
    rm -rf "${BUILD_DIR}"
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
  fi
fi

# -----------
# Configure
# -----------
CMAKE_ARGS=(
  -DCMAKE_BUILD_TYPE="${TYPE}"
  -DUSE_PKGCONFIG_SENSORS=OFF
  "-DSENSORS_INCLUDE_DIRS=${SENSORS_INCLUDE_DIRS}"
  "-DSENSORS_LIBRARIES=${SENSORS_LIBRARIES}"
)

if [[ -n "${GENERATOR}" ]]; then
  echo "[i] Using generator: ${GENERATOR}"
  cmake -G "${GENERATOR}" "${CMAKE_ARGS[@]}" ..
else
  cmake "${CMAKE_ARGS[@]}" ..
fi

# -------
# Build
# -------
cmake --build . -j

echo
echo "[i] Binaries:"
[[ -f ./lfcd ]] && echo "  - $(realpath ./lfcd)      (daemon)"
[[ -f ./lfc-gui ]] && echo "  - $(realpath ./lfc-gui)  (gui)"

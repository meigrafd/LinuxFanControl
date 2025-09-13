#!/usr/bin/env bash
# LinuxFanControl - build helper for the C++ daemon (and CMake tree)
# - Auto-detect libsensors (pkg-config or common paths)
# - Fresh builds via --fresh
# - Build types: Debug | Release | RelWithDebInfo | MinSizeRel
# - Optional: Ninja/Clang/Sanitizers
# - Clear summary of produced artifacts
set -euo pipefail

# ---------------------------
# Pretty print
# ---------------------------
c_reset=$'\033[0m'; c_bold=$'\033[1m'
c_green=$'\033[32m'; c_yellow=$'\033[33m'; c_red=$'\033[31m'; c_cyan=$'\033[36m'

log()  { printf "%s[i]%s %s\n" "$c_cyan" "$c_reset" "$*"; }
warn() { printf "%s[!]%s %s\n" "$c_yellow" "$c_reset" "$*"; }
err()  { printf "%s[x]%s %s\n" "$c_red" "$c_reset" "$*"; }

# ---------------------------
# Defaults
# ---------------------------
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT}/build"
BUILD_TYPE="RelWithDebInfo"
GEN=""
USE_CLANG=0
SANITIZE=""     # address|undefined|thread|leak
FRESH=0
EXTRA_CMAKE=()

# ---------------------------
# Args
# ---------------------------
usage() {
  cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --type <Debug|Release|RelWithDebInfo|MinSizeRel>   Build type (default: ${BUILD_TYPE})
  --fresh                                            Remove build dir before configuring
  --ninja                                            Use Ninja generator (if available)
  --clang                                            Build with clang/clang++
  --sanitize <address|undefined|thread|leak>         Enable sanitizer
  -D<VAR>=<VAL>                                      Extra -D flag passed to CMake (repeatable)
  --build-dir <path>                                 Custom build directory (default: build)
  --help                                             This help

Examples:
  $(basename "$0") --fresh --type RelWithDebInfo
  $(basename "$0") --ninja --clang -DSENSORS_INCLUDE_DIRS=/usr/include -DSENSORS_LIBRARIES=/usr/lib64/libsensors.so
EOF
}

while (( $# )); do
  case "$1" in
    --type) BUILD_TYPE="${2:-}"; shift;;
    --fresh) FRESH=1;;
    --ninja) GEN="Ninja";;
    --clang) USE_CLANG=1;;
    --sanitize) SANITIZE="${2:-}"; shift;;
    --build-dir) BUILD_DIR="${2:-}"; shift;;
    -D*) EXTRA_CMAKE+=("$1");;
    --help|-h) usage; exit 0;;
    *) err "Unknown argument: $1"; usage; exit 2;;
  esac
  shift
done

# ---------------------------
# Env / Tools
# ---------------------------
if [[ $USE_CLANG -eq 1 ]]; then
  export CC="${CC:-clang}"
  export CXX="${CXX:-clang++}"
  log "Using clang toolchain: CC=${CC} CXX=${CXX}"
fi

if [[ -n "${GEN}" ]]; then
  if ! command -v ninja >/dev/null 2>&1; then
    warn "Ninja requested but not found; falling back to default generator."
    GEN=""
  fi
fi

# ---------------------------
# libsensors autodetect
# ---------------------------
SENS_INC=""
SENS_LIB=""
detect_libsensors() {
  # 1) pkg-config
  if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists libsensors; then
    local incs libs
    incs=$(pkg-config --cflags-only-I libsensors | tr ' ' '\n' | sed -n 's/^-I//p' | head -n1 || true)
    libs=$(pkg-config --libs-only-L --libs-only-l libsensors || true)
    # Try to materialize .so
    for cand in \
      /usr/lib64/libsensors.so \
      /usr/lib/x86_64-linux-gnu/libsensors.so \
      /usr/lib/libsensors.so \
      /lib64/libsensors.so \
      /lib/x86_64-linux-gnu/libsensors.so \
      /lib/libsensors.so \
      ; do
      [[ -f "$cand" ]] && { SENS_LIB="$cand"; break; }
    done
    [[ -z "$SENS_LIB" ]] && SENS_LIB="$(pkg-config --variable=libdir libsensors 2>/dev/null)/libsensors.so"
    [[ -f "$SENS_LIB" ]] || SENS_LIB=""
    [[ -n "$incs" ]] && SENS_INC="$incs"
  fi

  # 2) Common fallbacks
  [[ -z "$SENS_INC" && -f /usr/include/sensors/sensors.h ]] && SENS_INC="/usr/include"
  for cand in \
    /usr/lib64/libsensors.so \
    /usr/lib/x86_64-linux-gnu/libsensors.so \
    /usr/lib/libsensors.so \
    /lib64/libsensors.so \
    /lib/x86_64-linux-gnu/libsensors.so \
    /lib/libsensors.so \
    ; do
    [[ -z "$SENS_LIB" && -f "$cand" ]] && SENS_LIB="$cand"
  done

  if [[ -n "${SENSORS_INCLUDE_DIRS:-}" ]]; then SENS_INC="${SENSORS_INCLUDE_DIRS}"; fi
  if [[ -n "${SENSORS_LIBRARIES:-}" ]]; then SENS_LIB="${SENSORS_LIBRARIES}"; fi
}

detect_libsensors

# ---------------------------
# Fresh build
# ---------------------------
log "Build type      : ${BUILD_TYPE}"
log "Root            : ${ROOT}"
log "Build dir       : ${BUILD_DIR}"
if [[ $FRESH -eq 1 ]]; then
  log "Fresh build requested -> removing ${BUILD_DIR}"
  rm -rf "${BUILD_DIR}"
fi
mkdir -p "${BUILD_DIR}"

# ---------------------------
# CMake configure
# ---------------------------
CMAKE_ARGS=(
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
)

# Sanitizers
if [[ -n "$SANITIZE" ]]; then
  CMAKE_ARGS+=(-DSANITIZE="${SANITIZE}")
  log "Sanitizer       : ${SANITIZE}"
fi

# libsensors, if found
if [[ -n "$SENS_INC" && -n "$SENS_LIB" ]]; then
  log "libsensors include: ${SENS_INC}"
  log "libsensors libs   : ${SENS_LIB}"
  CMAKE_ARGS+=(
    -DSENSORS_INCLUDE_DIRS="${SENS_INC}"
    -DSENSORS_LIBRARIES="${SENS_LIB}"
  )
else
  warn "libsensors not found (headers/lib). The daemon will fallback to /sys/class/hwmon."
fi

# Extra -D flags
if (( ${#EXTRA_CMAKE[@]} )); then
  log "Extra CMake     : ${EXTRA_CMAKE[*]}"
  CMAKE_ARGS+=("${EXTRA_CMAKE[@]}")
fi

# Generator
if [[ -n "$GEN" ]]; then
  log "Generator       : ${GEN}"
  cmake -S "${ROOT}" -B "${BUILD_DIR}" -G "${GEN}" "${CMAKE_ARGS[@]}"
else
  cmake -S "${ROOT}" -B "${BUILD_DIR}"            "${CMAKE_ARGS[@]}"
fi

# ---------------------------
# Build
# ---------------------------
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
cmake --build "${BUILD_DIR}" -j "${JOBS}"

echo
log "Binaries:"
if [[ -f "${BUILD_DIR}/lfcd" ]]; then
  printf "  - %s\n" "$(realpath "${BUILD_DIR}/lfcd")"
else
  warn "lfcd not found in ${BUILD_DIR} (did configuration fail?)"
fi

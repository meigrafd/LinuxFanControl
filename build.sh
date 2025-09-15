#!/usr/bin/env bash
# LinuxFanControl - unified build helper for Daemon (C++) + GUI (.NET 9)
# - Auto-detect libsensors (pkg-config or common paths)
# - Fresh builds (--fresh or FRESH=1)
# - Daemon options: --type, --ninja, --clang, --sanitize, -D...
# - GUI options   : --gui-config, --rid, --publish, --self-contained
# - Targets       : --daemon-only | --gui-only (default: build both)
# - Creates a dist/ folder with daemon + GUI and a start script

# --- Always re-exec under bash (fixes "ran under sh" issues) ---
if [ -z "${BASH_VERSION:-}" ]; then exec /usr/bin/env bash "$0" "$@"; fi

# --- Strict mode + helpful error line on failure ---
#set -Eeuo pipefail
trap 'echo "[x] build.sh failed at line $LINENO (last cmd: $BASH_COMMAND)"; exit 1' ERR


# ---------- Pretty print ----------
c_reset=$'\033[0m'; c_bold=$'\033[1m'
c_green=$'\033[32m'; c_yellow=$'\033[33m'; c_red=$'\033[31m'; c_cyan=$'\033[36m'
log()  { printf "%s[i]%s %s\n" "$c_cyan" "$c_reset" "$*"; }
warn() { printf "%s[!]%s %s\n" "$c_yellow" "$c_reset" "$*"; }
err()  { printf "%s[x]%s %s\n" "$c_red" "$c_reset" "$*"; }

# ---------- Defaults ----------
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT}/build"
DIST_DIR="${ROOT}/dist"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"

# Daemon (CMake/C++)
BUILD_TYPE="RelWithDebInfo"
GEN=""            # Ninja if requested and available
USE_CLANG=0
SANITIZE=""       # address|undefined|thread|leak
EXTRA_CMAKE=()

# GUI (.NET/Avalonia)
GUI_CFG="Debug"   # or Release
RID_DEFAULT="linux-x64"
GUI_RID="$RID_DEFAULT"
GUI_PUBLISH=0
GUI_SELF_CONTAINED=0

# Orchestration
FRESH="${FRESH:-0}"   # <-- respect env var FRESH=1
ONLY_DAEMON=0
ONLY_GUI=0

# Paths (GUI project will be auto-detected below)
GUI_PROJ=""
DAEMON_BIN=""

# ---------- Usage ----------
usage() {
  cat <<EOF
Usage: $(basename "$0") [options]

General:
  --fresh                         Remove build dir (and dist if publishing) before building
  --daemon-only                   Build only the C++ daemon
  --gui-only                      Build only the Avalonia GUI

C++ Daemon (CMake):
  --type <Debug|Release|RelWithDebInfo|MinSizeRel>   Build type (default: ${BUILD_TYPE})
  --ninja                        Use Ninja generator (if available)
  --clang                        Use clang/clang++
  --sanitize <address|undefined|thread|leak>
  -D<VAR>=<VAL>                  Extra -D flag passed to CMake (repeatable)

GUI:
  --gui-config <Debug|Release>   GUI configuration (default: ${GUI_CFG})
  --rid <RID>                    Runtime identifier (default: ${RID_DEFAULT})
  --publish                      dotnet publish (otherwise dotnet build)
  --self-contained               Publish self-contained (implies --publish)
EOF
}

# ---------- Parse args ----------
while (( $# )); do
  case "$1" in
    --fresh) FRESH=1 ;;
    --daemon-only) ONLY_DAEMON=1 ;;
    --gui-only) ONLY_GUI=1 ;;
    --type) BUILD_TYPE="${2:-}"; shift ;;
    --ninja) GEN="Ninja" ;;
    --clang) USE_CLANG=1 ;;
    --sanitize) SANITIZE="${2:-}"; shift ;;
    -D*) EXTRA_CMAKE+=("$1") ;;
    --gui-config) GUI_CFG="${2:-}"; shift ;;
    --rid) GUI_RID="${2:-}"; shift ;;
    --publish) GUI_PUBLISH=1 ;;
    --self-contained) GUI_PUBLISH=1; GUI_SELF_CONTAINED=1 ;;
    --help|-h) usage; exit 0 ;;
    *) err "Unknown arg: $1"; usage; exit 2 ;;
  esac
  shift
done

# ---------- Tools ----------
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

if [[ $ONLY_DAEMON -eq 1 && $ONLY_GUI -eq 1 ]]; then
  err "Choose at most one of --daemon-only or --gui-only."
  exit 2
fi

# ---------- libsensors autodetect ----------
SENS_INC=""; SENS_LIB=""
detect_libsensors() {
  # pkg-config
  if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists libsensors; then
    local incs
    incs=$(pkg-config --cflags-only-I libsensors | tr ' ' '\n' | sed -n 's/^-I//p' | head -n1 || true)
    [[ -n "$incs" ]] && SENS_INC="$incs"
  fi
  [[ -z "$SENS_INC" && -f /usr/include/sensors/sensors.h ]] && SENS_INC="/usr/include"
  for cand in \
    /usr/lib64/libsensors.so \
    /usr/lib/x86_64-linux-gnu/libsensors.so \
    /usr/lib/libsensors.so \
    /lib64/libsensors.so \
    /lib/x86_64-linux-gnu/libsensors.so \
    /lib/libsensors.so; do
    [[ -z "$SENS_LIB" && -f "$cand" ]] && SENS_LIB="$cand"
  done
  [[ -n "${SENSORS_INCLUDE_DIRS:-}" ]] && SENS_INC="${SENSORS_INCLUDE_DIRS}"
  [[ -n "${SENSORS_LIBRARIES:-}"    ]] && SENS_LIB="${SENSORS_LIBRARIES}"
}

# ---------- find lfcd helper ----------
find_daemon_bin() {
  local candidates=(
    "${BUILD_DIR}/lfcd"
    "${BUILD_DIR}/src/daemon/lfcd"
    "${BUILD_DIR}/src/daemon/src/lfcd"
  )
  for c in "${candidates[@]}"; do
    [[ -x "$c" ]] && { DAEMON_BIN="$c"; return; }
  done
  local found
  found="$(find "${BUILD_DIR}" -maxdepth 5 -type f -name lfcd 2>/dev/null | head -n1 || true)"
  [[ -n "$found" ]] && DAEMON_BIN="$found" || DAEMON_BIN=""
}

# ---------- copy helper (rsync fallback) ----------
copy_tree() {
  local src="$1" dst="$2"
  if command -v rsync >/dev/null 2>&1; then
    rsync -a --delete "${src}/" "${dst}/"
  else
    mkdir -p "${dst}"
    ( cd "${src}" && find . -type d -exec mkdir -p "${dst}/{}" \; )
    ( cd "${src}" && find . -type f -exec cp -f "{}" "${dst}/{}" \; )
  fi
}

# ---------- GUI project autodetect ----------
detect_gui_project() {
  # preferred current layout
  if [[ -f "${ROOT}/src/gui-avalonia/LinuxFanControl.Gui/LinuxFanControl.Gui.csproj" ]]; then
    GUI_PROJ="${ROOT}/src/gui-avalonia/LinuxFanControl.Gui/LinuxFanControl.Gui.csproj"
    return
  fi
  # legacy path
  if [[ -f "${ROOT}/src/gui/gui.csproj" ]]; then
    GUI_PROJ="${ROOT}/src/gui/gui.csproj"
    return
  fi
  # fallback: first .csproj in tree (maxdepth limit to be safe)
  GUI_PROJ="$(find "${ROOT}" -maxdepth 5 -type f -name "*.csproj" -print -quit || true)"
}

# ---------- Build Daemon ----------
build_daemon() {
  detect_libsensors
  log "Build type      : ${BUILD_TYPE}"
  log "Root            : ${ROOT}"
  log "Build dir       : ${BUILD_DIR}"
  if [[ $FRESH -eq 1 ]]; then
    log "Fresh build requested -> removing ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
  fi
  mkdir -p "${BUILD_DIR}"

  local cmake_args=(
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
  )
  if [[ -n "${SANITIZE}" ]]; then
    cmake_args+=(-DSANITIZE="${SANITIZE}")
    log "Sanitizer       : ${SANITIZE}"
  fi
  if [[ -n "${SENS_INC}" && -n "${SENS_LIB}" ]]; then
    log "libsensors include: ${SENS_INC}"
    log "libsensors libs   : ${SENS_LIB}"
    cmake_args+=(
      -DSENSORS_INCLUDE_DIRS="${SENS_INC}"
      -DSENSORS_LIBRARIES="${SENS_LIB}"
    )
  else
    warn "libsensors not found (headers/lib). Daemon will fallback to /sys/class/hwmon."
  fi
  if (( ${#EXTRA_CMAKE[@]} )); then
    log "Extra CMake     : ${EXTRA_CMAKE[*]}"
    cmake_args+=("${EXTRA_CMAKE[@]}")
  fi

  if [[ -n "${GEN}" ]]; then
    log "Generator       : ${GEN}"
    cmake -S "${ROOT}" -B "${BUILD_DIR}" -G "${GEN}" "${cmake_args[@]}"
  else
    cmake -S "${ROOT}" -B "${BUILD_DIR}"            "${cmake_args[@]}"
  fi

  cmake --build "${BUILD_DIR}" -j "${JOBS}"

  find_daemon_bin
  if [[ -x "${DAEMON_BIN}" ]]; then
    log "Daemon built    : $(realpath "${DAEMON_BIN}")"
  else
    err "Daemon binary not found in ${BUILD_DIR}."
    exit 3
  fi
}

# ---------- Build GUI ----------
need_dotnet() {
  if ! command -v dotnet >/dev/null 2>&1; then
    err "dotnet not found. Please install .NET 9 SDK."
    exit 4
  fi
}

gui_output_dir() {
  local proj_dir; proj_dir="$(dirname "${GUI_PROJ}")"
  local base="${proj_dir}/bin/${GUI_CFG}/net9.0"
  if [[ $GUI_PUBLISH -eq 1 ]]; then
    echo "${base}/${GUI_RID}/publish"
  else
    echo "${base}"
  fi
}

build_gui() {
  need_dotnet
  detect_gui_project
  if [[ -z "${GUI_PROJ}" || ! -f "${GUI_PROJ}" ]]; then
    err "GUI project not found. Expected: src/gui-avalonia/LinuxFanControl.Gui/LinuxFanControl.Gui.csproj"
    exit 5
  fi

  log "GUI project     : ${GUI_PROJ#${ROOT}/}"
  log "GUI config      : ${GUI_CFG}"
  log "GUI RID         : ${GUI_RID}"
  log "GUI publish     : $([[ $GUI_PUBLISH -eq 1 ]] && echo yes || echo no)"
  log "Self-contained  : $([[ $GUI_SELF_CONTAINED -eq 1 ]] && echo yes || echo no)"

  ( cd "$(dirname "${GUI_PROJ}")"
    dotnet restore
    if [[ $GUI_PUBLISH -eq 1 ]]; then
      if [[ $GUI_SELF_CONTAINED -eq 1 ]]; then
        dotnet publish -c "${GUI_CFG}" -r "${GUI_RID}" --self-contained true
      else
        dotnet publish -c "${GUI_CFG}" -r "${GUI_RID}" --self-contained false
      fi
    else
      dotnet build -c "${GUI_CFG}"
    fi
  )

  local out; out="$(gui_output_dir)"
  log "GUI output      : ${out}"

  # Prepare dist/ when publishing
  if [[ $GUI_PUBLISH -eq 1 ]]; then
    if [[ $FRESH -eq 1 ]]; then
      log "Fresh dist -> removing ${DIST_DIR}"
      rm -rf "${DIST_DIR}"
    fi
    mkdir -p "${DIST_DIR}"
    copy_tree "${out}" "${DIST_DIR}"
    # Copy daemon
    find_daemon_bin
    if [[ -x "${DAEMON_BIN}" ]]; then
      cp -f "${DAEMON_BIN}" "${DIST_DIR}/lfcd"
      chmod +x "${DIST_DIR}/lfcd"
    fi
    # start script in dist
    cat > "${DIST_DIR}/start.sh" <<'EOS'
#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LFC_DAEMON="${HERE}/lfcd"
export LFC_DAEMON_ARGS="${LFC_DAEMON_ARGS:-}"
LOG_DIR="/tmp"
# start daemon in background if not running
if ! pgrep -x lfcd >/dev/null 2>&1; then
  setsid bash -c "exec '${LFC_DAEMON}' ${LFC_DAEMON_ARGS} >>'${LOG_DIR}/daemon_lfc.log' 2>&1" </dev/null &
  disown
fi
# run GUI
exec "${HERE}/LinuxFanControl.Gui" "$@"
EOS
    chmod +x "${DIST_DIR}/start.sh"
    log "Dist ready      : ${DIST_DIR} (run ./start.sh)"
  else
    warn "No publish requested: run GUI from project with \`dotnet run -c ${GUI_CFG}\`."
  fi
}

# ---------- Main ----------
if [[ $ONLY_GUI -eq 1 ]]; then
  build_gui
elif [[ $ONLY_DAEMON -eq 1 ]]; then
  build_daemon
else
  build_daemon
  build_gui
fi

echo
log "Summary:"
find_daemon_bin
[[ -x "${DAEMON_BIN}" ]] && echo "  Daemon: $(realpath "${DAEMON_BIN}")"
if [[ -d "${DIST_DIR}" ]]; then
  echo "  Dist  : $(realpath "${DIST_DIR}")"
  [[ -f "${DIST_DIR}/start.sh" ]] && echo "          Run: ${DIST_DIR}/start.sh"
fi

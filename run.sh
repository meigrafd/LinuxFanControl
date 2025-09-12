#!/usr/bin/env bash
set -euo pipefail

# Linux Fan Control - developer runner
# - Builds (with optional fresh clean)
# - Runs GUI, which in turn spawns daemon under chosen wrapper (gdb/valgrind/strace)
# - Captures logs to ./logs
# - Provides quick helpers: logs, tail, clean

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT}/build"
LOG_DIR="/tmp"

LFCD="${BUILD_DIR}/lfcd"
LFCGUI="${BUILD_DIR}/lfc-gui"

# -------- colors ----------
c_info="\033[1;34m"
c_ok="\033[1;32m"
c_warn="\033[1;33m"
c_err="\033[1;31m"
c_off="\033[0m"

say() { echo -e "${c_info}[i]${c_off} $*"; }
ok()  { echo -e "${c_ok}[✓]${c_off} $*"; }
warn(){ echo -e "${c_warn}[!]${c_off} $*"; }
err() { echo -e "${c_err}[x]${c_off} $*" >&2; }

usage() {
  cat <<EOF
Usage: $0 <cmd> [options]

Commands:
  build            Build project (add --fresh for clean configure)
  gui              Run GUI (it spawns daemon). Options:
                     --gdb | --valgrind | --strace
                     --theme=dark|light
                     --args="..."      (extra args for GUI)
                     --daemon-args="..." (extra args for daemon)
  logs             Show log file paths
  tail             Tail both logs
  clean            Remove build directory

Environment overrides (optional):
  LFC_DAEMON            Path to lfcd (default: build/lfcd)
  LFC_DAEMON_ARGS       Extra args for daemon (default: --debug)
  LFC_DAEMON_WRAPPER    Wrapper to prefix daemon (e.g. "gdb -ex run --args")
  LFC_GUI_DEBUG         1 to enable verbose GUI debug prints (default: 1)

Examples:
  $0 build --fresh
  $0 gui --gdb --theme=dark
  $0 gui --valgrind --daemon-args="--debug"
  $0 tail
EOF
}

# ---------- helpers ----------
do_build() {
  local fresh=0
  [[ "${1:-}" == "--fresh" ]] && fresh=1
  say "Build type      : RelWithDebInfo"
  say "Root            : ${ROOT}"
  say "Build dir       : ${BUILD_DIR}"
  if (( fresh )); then
    say "Fresh build requested -> removing ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
  fi
  mkdir -p "${BUILD_DIR}"
  pushd "${BUILD_DIR}" >/dev/null
  cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
  cmake --build . -j
  popd >/dev/null
  [[ -x "${LFCD}" ]] && ok "Daemon: ${LFCD}" || { err "lfcd missing"; exit 2; }
  [[ -x "${LFCGUI}" ]] && ok "GUI   : ${LFCGUI}" || { err "lfc-gui missing"; exit 2; }
}

do_logs() {
  echo "${LOG_DIR}/daemon_lfc.log"
  echo "${LOG_DIR}/gui_lfc.log"
}

do_tail() {
  touch "${LOG_DIR}/daemon_lfc.log" "${LOG_DIR}/gui_lfc.log"
  tail -n 100 -F "${LOG_DIR}/daemon_lfc.log" "${LOG_DIR}/gui_lfc.log"
}

do_clean() {
  say "Removing build dir: ${BUILD_DIR}"
  rm -rf "${BUILD_DIR}"
  ok "Done."
}

do_gui() {
  # defaults
  local theme="dark"
  local gui_args=""
  local daemon_args="${LFC_DAEMON_ARGS:---debug}"
  local wrapper=""

  while [[ $# -gt 0 ]]; do
    case "$1" in
      --gdb)       wrapper="gdb -ex run --args"; shift ;;
      --valgrind)  wrapper="valgrind --leak-check=full --show-leak-kinds=all"; shift ;;
      --strace)    wrapper="strace -f -s 200 -o ${LOG_DIR}/daemon_lfc.strace"; shift ;;
      --theme=*)   theme="${1#*=}"; shift ;;
      --args=*)    gui_args="${1#*=}"; shift ;;
      --daemon-args=*) daemon_args="${1#*=}"; shift ;;
      *) err "Unknown option: $1"; usage; exit 1 ;;
    esac
  done

  [[ -x "${LFCD}" ]]  || { err "Daemon not found: ${LFCD}"; exit 2; }
  [[ -x "${LFCGUI}" ]]|| { err "GUI not found: ${LFCGUI}"; exit 2; }

  # Export env so GUI can spawn daemon consistently
  export LFC_DAEMON="${LFC_DAEMON:-${LFCD}}"
  export LFC_DAEMON_ARGS="${daemon_args}"
  export LFC_DAEMON_WRAPPER="${LFC_DAEMON_WRAPPER:-${wrapper}}"
  export LFC_GUI_DEBUG="${LFC_GUI_DEBUG:-1}"
  export LFC_THEME="${theme}"

  say "Daemon: ${LFC_DAEMON}"
  say "Wrapper: ${LFC_DAEMON_WRAPPER:-<none>}"
  say "Daemon args: ${LFC_DAEMON_ARGS}"
  say "GUI debug: ${LFC_GUI_DEBUG} | Theme: ${LFC_THEME}"
  say "Logs: ${LOG_DIR}/daemon_lfc.log , ${LOG_DIR}/gui_lfc.log"

  # Prepare log redirection: GUI will spawn daemon; we request both to write logs.
  # Many projects already log to stderr; we tee here to files.
  # If your GUI/daemon already write to logs internally, these tees are still helpful for live view.

  # Clean old logs
  : > "${LOG_DIR}/daemon_lfc.log"
  : > "${LOG_DIR}/gui_lfc.log"

  # Run GUI (which spawns daemon) and tee its output
  # We also pass theme via env; your GUI can read LFC_THEME to choose light/dark.
  (
    cd "${ROOT}"
    # shellcheck disable=SC2086
    "${LFCGUI}" ${gui_args}
  ) > >(tee -a "${LOG_DIR}/gui_lfc.log") 2> >(tee -a "${LOG_DIR}/gui_lfc.log" >&2)
}

# ---------- main ----------
cmd="${1:-}"
shift || true

case "${cmd}" in
  build) do_build "$@";;
  gui)   do_gui "$@";;
  logs)  do_logs;;
  tail)  do_tail;;
  clean) do_clean;;
  ""|-h|--help) usage;;
  *) err "Unknown command: ${cmd}"; usage; exit 1;;
esac

#!/usr/bin/env bash
# Linux Fan Control - runtime helper (no build)
# Starts GUI (which spawns the daemon) or starts the daemon standalone.
# Logs are written to /tmp/daemon_lfc.log and /tmp/gui_lfc.log.
# (c) 2025 meigrafd & contributors - MIT

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT}/build"

# Log locations (as requested)
LOG_DIR="/tmp"
LOG_DAEMON="${LOG_DIR}/daemon_lfc.log"
LOG_GUI="${LOG_DIR}/gui_lfc.log"
LOG_STRACE="${LOG_DIR}/daemon_lfc.strace"

LFCD_DEFAULT="${BUILD_DIR}/lfcd"
LFCGUI_DEFAULT="${BUILD_DIR}/lfc-gui"

# ---- tiny helpers (colors) ----
c_info="\033[1;34m"; c_ok="\033[1;32m"; c_warn="\033[1;33m"; c_err="\033[1;31m"; c_off="\033[0m"
say(){ echo -e "${c_info}[i]${c_off} $*"; }
ok(){  echo -e "${c_ok}[✓]${c_off} $*"; }
warn(){echo -e "${c_warn}[!]${c_off} $*"; }
err(){ echo -e "${c_err}[x]${c_off} $*" >&2; }

usage() {
  cat <<EOF
Usage: $0 <command> [options]

Commands:
  gui [opts]         Run GUI (it spawns the daemon)
     --gdb | --valgrind | --strace
     --theme=dark|light         (exported as LFC_THEME, default: dark)
     --args="..."               (extra args to lfc-gui)
     --daemon-args="--debug"    (extra args to lfcd)
  daemon [opts]      Run daemon only (useful for debugging)
     --gdb | --valgrind | --strace
     --args="--debug"
  logs               Print log paths
  tail               Tail logs

Environment overrides:
  LFC_DAEMON            Path to lfcd (default: ${LFCD_DEFAULT})
  LFC_DAEMON_ARGS       Extra args for daemon (default: --debug)
  LFC_DAEMON_WRAPPER    e.g. "gdb -ex run --args" or "valgrind --leak-check=full"
  LFC_GUI               Path to lfc-gui (default: ${LFCGUI_DEFAULT})
  LFC_GUI_DEBUG         1 enables verbose GUI logs (default: 1)
  LFC_THEME             dark|light (default: dark)

Note: This script does NOT build. Run ./build.sh beforehand.

Log files:
  ${LOG_DAEMON}
  ${LOG_GUI}
EOF
}

need_file() {
  local f="$1" what="$2"
  [[ -x "$f" ]] && return 0
  err "$what not found at: $f"
  err "Build first, e.g.: ./build.sh"
  exit 2
}

do_logs(){ echo "${LOG_DAEMON}"; echo "${LOG_GUI}"; }
do_tail(){ touch "${LOG_DAEMON}" "${LOG_GUI}"; tail -n 100 -F "${LOG_DAEMON}" "${LOG_GUI}"; }

do_gui() {
  local theme="${LFC_THEME:-dark}"
  local gui_args=""
  local daemon_args="${LFC_DAEMON_ARGS:---debug}"
  local wrapper=""

  while [[ $# -gt 0 ]]; do
    case "$1" in
      --gdb)       wrapper="gdb -ex run --args"; shift ;;
      --valgrind)  wrapper="valgrind --leak-check=full --show-leak-kinds=all"; shift ;;
      --strace)    wrapper="strace -f -s 200 -o ${LOG_STRACE}"; shift ;;
      --theme=*)   theme="${1#*=}"; shift ;;
      --args=*)    gui_args="${1#*=}"; shift ;;
      --daemon-args=*) daemon_args="${1#*=}"; shift ;;
      *) err "Unknown option: $1"; usage; exit 1 ;;
    esac
  done

  local LFCGUI="${LFC_GUI:-${LFCGUI_DEFAULT}}"
  local LFCD="${LFC_DAEMON:-${LFCD_DEFAULT}}"

  need_file "$LFCGUI" "GUI binary"
  need_file "$LFCD" "Daemon binary"

  # Export env so GUI can spawn the daemon consistently.
  export LFC_DAEMON="$LFCD"
  export LFC_DAEMON_ARGS="$daemon_args"
  export LFC_DAEMON_WRAPPER="${LFC_DAEMON_WRAPPER:-$wrapper}"
  export LFC_GUI_DEBUG="${LFC_GUI_DEBUG:-1}"
  export LFC_THEME="$theme"

  say "GUI   : $LFCGUI"
  say "Daemon: $LFC_DAEMON"
  say "Wrap  : ${LFC_DAEMON_WRAPPER:-<none>}"
  say "Args  : GUI(${gui_args})  |  Daemon(${LFC_DAEMON_ARGS})"
  say "Theme : $LFC_THEME"
  say "Logs  : ${LOG_GUI} , ${LOG_DAEMON}"

  : > "${LOG_GUI}"
  : > "${LOG_DAEMON}"

  (
    cd "${ROOT}"
    # shellcheck disable=SC2086
    "$LFCGUI" ${gui_args}
  ) > >(tee -a "${LOG_GUI}") 2> >(tee -a "${LOG_GUI}" >&2)
}

do_daemon() {
  local daemon_args="${LFC_DAEMON_ARGS:---debug}"
  local wrapper=""

  while [[ $# -gt 0 ]]; do
    case "$1" in
      --gdb)       wrapper="gdb -ex run --args"; shift ;;
      --valgrind)  wrapper="valgrind --leak-check=full --show-leak-kinds=all"; shift ;;
      --strace)    wrapper="strace -f -s 200 -o ${LOG_STRACE}"; shift ;;
      --args=*)    daemon_args="${1#*=}"; shift ;;
      *) err "Unknown option: $1"; usage; exit 1 ;;
    esac
  done

  local LFCD="${LFC_DAEMON:-${LFCD_DEFAULT}}"
  need_file "$LFCD" "Daemon binary"

  say "Daemon: $LFCD"
  say "Wrap  : ${wrapper:-<none>}"
  say "Args  : ${daemon_args}"
  say "Log   : ${LOG_DAEMON}"

  : > "${LOG_DAEMON}"

  if [[ -n "$wrapper" ]]; then
    # Run with wrapper via shell
    /bin/sh -c "${wrapper} $LFCD ${daemon_args}" \
      > >(tee -a "${LOG_DAEMON}") 2> >(tee -a "${LOG_DAEMON}" >&2)
  else
    # shellcheck disable=SC2086
    "$LFCD" ${daemon_args} \
      > >(tee -a "${LOG_DAEMON}") 2> >(tee -a "${LOG_DAEMON}" >&2)
  fi
}

cmd="${1:-}"; shift || true
case "$cmd" in
  gui)   do_gui "$@";;
  daemon)do_daemon "$@";;
  logs)  do_logs;;
  tail)  do_tail;;
  ""|-h|--help) usage;;
  *) err "Unknown command: $cmd"; usage; exit 1;;
esac

#!/usr/bin/env bash
# LinuxFanControl - runtime helper (no build here)
# - Starts daemon or GUI
# - Logs to /tmp/daemon_lfc.log and /tmp/gui_lfc.log
# - Single-instance protection for daemon
# - Optional debug flags via env: LFCD_DAEMON_ARGS="--debug"

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="/tmp"

# Auto-detect daemon binary
detect_daemon() {
    local candidates=(
        "${ROOT}/build/lfcd"
        "${ROOT}/build/src/daemon/lfcd"
        "${ROOT}/build/src/daemon/src/lfcd"
    )
    for c in "${candidates[@]}"; do
        [[ -x "$c" ]] && { echo "$c"; return; }
    done
    local f
    f="$(find "${ROOT}/build" -maxdepth 5 -type f -name lfcd 2>/dev/null | head -n1 || true)"
    echo "${f}"
}

DAEMON_DEFAULT="$(detect_daemon)"
DAEMON="${LFCD_DAEMON:-${DAEMON_DEFAULT}}"
DAEMON_ARGS="${LFCD_DAEMON_ARGS:-}"
GUI_DIR="${ROOT}/src/gui-avalonia/LinuxFanControl.Gui"
GUI_CMD="dotnet run -c Debug"

usage() {
    cat <<EOF
Usage: $(basename "$0") [daemon|gui|stop-daemon|status]

  daemon       Start daemon (stdio server); log -> ${LOG_DIR}/daemon_lfc.log
  gui          Start Avalonia GUI;          log -> ${LOG_DIR}/gui_lfc.log
  stop-daemon  Stop running daemon (by pid)
  status       Show daemon status

Env:
  LFCD_DAEMON=path/to/lfcd
  LFCD_DAEMON_ARGS="--debug"
EOF
}

pidfile() { echo "${LOG_DIR}/lfcd.pid"; }

daemon_running() {
    local pf; pf="$(pidfile)"
    [[ -f "$pf" ]] || return 1
    local pid; pid="$(cat "$pf" 2>/dev/null || true)"
    [[ -n "${pid}" && -d "/proc/${pid}" ]] || return 1
    kill -0 "${pid}" >/dev/null 2>&1 || return 1
    return 0
}

start_daemon() {
    if [[ -z "${DAEMON}" || ! -x "${DAEMON}" ]]; then
        echo "[!] Daemon binary not found. Build first or set LFCD_DAEMON."
        exit 1
    fi
    if daemon_running; then
        echo "[i] Daemon already running (pid $(cat "$(pidfile)")). Use 'stop-daemon' first."
        exit 0
    fi
    echo "[i] Starting daemon: ${DAEMON} ${DAEMON_ARGS}"
    setsid bash -c "exec '${DAEMON}' ${DAEMON_ARGS} >>'${LOG_DIR}/daemon_lfc.log' 2>&1" </dev/null &
    disown
    echo $! >"$(pidfile)"
    echo "[i] PID: $(cat "$(pidfile)")"
    echo "[i] Tail log: tail -f ${LOG_DIR}/daemon_lfc.log"
}

stop_daemon() {
    if ! daemon_running; then
        echo "[i] Daemon not running."
        rm -f "$(pidfile)" || true
        exit 0
    fi
    local pid; pid="$(cat "$(pidfile)")"
    echo "[i] Stopping daemon pid ${pid} ..."
    kill "${pid}" 2>/dev/null || true
    sleep 0.3
    if kill -0 "${pid}" 2>/dev/null; then
        echo "[i] Force kill..."
        kill -9 "${pid}" 2>/dev/null || true
    fi
    rm -f "$(pidfile)" || true
    echo "[i] Stopped."
}

status_daemon() {
    if daemon_running; then
        echo "[i] Daemon running (pid $(cat "$(pidfile)"))."
    else
        echo "[i] Daemon not running."
    fi
}

start_gui() {
    export LFCD_DAEMON="${DAEMON}"
    export LFCD_DAEMON_ARGS="${DAEMON_ARGS}"
    echo "[i] Starting GUI (Avalonia) -> log ${LOG_DIR}/gui_lfc.log"
    ( cd "${GUI_DIR}" && ${GUI_CMD} ) >>"${LOG_DIR}/gui_lfc.log" 2>&1
}

cmd="${1:-gui}"
case "$cmd" in
    daemon)       start_daemon ;;
    gui)          start_gui ;;
    stop-daemon)  stop_daemon ;;
    status)       status_daemon ;;
    *)            usage; exit 2 ;;
esac

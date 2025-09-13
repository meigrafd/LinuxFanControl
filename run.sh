#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="/tmp"
DAEMON="$ROOT/build/lfcd"

cmd="${1:-gui}"
case "$cmd" in
  daemon)
    [[ -x "$DAEMON" ]] || { echo "[!] build daemon first"; exit 1; }
    exec "$DAEMON" --debug 2>&1 | tee "$LOG_DIR/daemon_lfc.log"
    ;;
  gui)
    export LFC_DAEMON="${DAEMON}"
    export LFC_DAEMON_ARGS="--debug"
    ( cd "$ROOT/src/gui-avalonia/LinuxFanControl.Gui"
      dotnet run -c Debug 2>&1 | tee "$LOG_DIR/gui_lfc.log"
    )
    ;;
  *)
    echo "Usage: $0 [daemon|gui]"
    exit 1
    ;;
esac

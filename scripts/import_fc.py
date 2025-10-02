#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Linux Fan Control — FanControl.Release Import & Live Progress (persistent JSON-RPC)
Leise Ausgabe: einzeilige, gedrosselte Progress-Anzeige; optionales Logfile.

Usage (Beispiel):
  python3 import_fc.py -f /mnt/Github/userConfig.json -C -E
  python3 import_fc.py -f ./userConfig.json -C -E --log-file import.log --progress-every-ms 500
"""

import argparse
import json
import socket
import sys
import time
from typing import Optional, Tuple, Dict, Any, List, Union, TextIO
from pathlib import Path

# ----------------------------- CLI -----------------------------------------

def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="FanControl.Release Import & Progress (persistent JSON-RPC, quiet output)"
    )
    p.add_argument("-f", "--file", required=True, help="Path to FanControl.Release JSON (userConfig.json)")
    p.add_argument("-n", "--name", default="Imported", help='Profile name (default: "Imported")')
    p.add_argument("-V", "--validate-detect", action="store_true", help="Enable deterministic validation step")
    p.add_argument("-r", "--rpm-min", type=int, default=0, help="Min RPM threshold (default: 0)")
    p.add_argument("-T", "--timeout-ms", type=int, default=0, help="Threshold timeout (ms, default: 0)")
    p.add_argument("-C", "--commit", action="store_true", help="Commit imported profile after success")
    p.add_argument("-E", "--enable", dest="enable_engine", action="store_true", help="Enable engine after commit")
    p.add_argument("-H", "--host", default="127.0.0.1", help="RPC host (default: 127.0.0.1)")
    p.add_argument("-p", "--port", type=int, default=8777, help="RPC port (default: 8777)")
    p.add_argument("-I", "--interval-ms", type=int, default=100, help="Poll interval ms (default: 100)")
    p.add_argument("-i", "--id-start", type=int, default=1, help="Starting JSON-RPC id (default: 1)")
    p.add_argument("-q", "--quiet", action="store_true", help="Quiet mode (nur Fehler)")
    p.add_argument("--no-color", action="store_true", help="Disable colored output")
    p.add_argument("-J", "--pretty-json", action="store_true", help="Pretty-print raw JSON replies to log file (nicht Konsole)")
    p.add_argument("--debug", action="store_true", help="Debug raw request/response (nur Logfile)")
    p.add_argument("--log-file", default=None, help="Schreibt Debug/Raw/Detail in diese Datei statt Konsole")
    p.add_argument("--progress-every-ms", type=int, default=1000, help="Spätestens alle X ms Progress-Zeile aktualisieren (default: 1000)")
    return p.parse_args()

# ----------------------------- Util -----------------------------------------

def ts() -> str:
    return time.strftime("%H:%M:%S")

class Color:
    def __init__(self, enabled: bool):
        if enabled and sys.stderr.isatty():
            self.B="\033[1m"; self.R="\033[31m"; self.G="\033[32m"; self.Y="\033[33m"; self.C="\033[36m"; self.N="\033[0m"
        else:
            self.B=self.R=self.G=self.Y=self.C=self.N=""

def open_logfile(path: Optional[str]) -> Optional[TextIO]:
    if not path:
        return None
    p = Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    return p.open("a", encoding="utf-8")

def log_line(msg: str, quiet: bool, stream: TextIO = sys.stderr):
    if not quiet:
        stream.write(msg + ("\n" if not msg.endswith("\n") else ""))
        stream.flush()

def log_dbg(msg: str, logf: Optional[TextIO]):
    if logf:
        logf.write(f"[{ts()}] {msg}\n")
        logf.flush()

# ----------------------- JSON helpers (robust fields) -----------------------

def try_paths(obj: Dict[str, Any], paths: List[str]) -> Optional[Any]:
    """
    Try multiple dotted paths like 'result.data.state' against dict `obj`.
    Returns the first found scalar or None.
    """
    for path in paths:
        cur: Any = obj
        ok = True
        for key in path.split("."):
            if isinstance(cur, dict) and key in cur:
                cur = cur[key]
            else:
                ok = False
                break
        if ok and not isinstance(cur, (dict, list)):
            return cur
    return None

# ------------------------- Persistent JSON-RPC client ------------------------

class RpcClient:
    def __init__(self, host: str, port: int, debug: bool=False, pretty: bool=False, logf: Optional[TextIO]=None):
        self.host = host
        self.port = port
        self.sock: Optional[socket.socket] = None
        self.debug = debug
        self.pretty = pretty
        self.logf = logf
        self.rpc_id = 1

    def set_start_id(self, i: int):
        self.rpc_id = i

    def connect(self) -> None:
        self.close()
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
        s.settimeout(10.0)
        s.connect((self.host, self.port))
        self.sock = s
        log_dbg(f"Connected to {self.host}:{self.port}", self.logf)

    def close(self) -> None:
        if self.sock:
            try:
                self.sock.shutdown(socket.SHUT_RDWR)
            except Exception:
                pass
            try:
                self.sock.close()
            except Exception:
                pass
        self.sock = None

    def _send_line(self, line: str) -> None:
        if not self.sock:
            raise RuntimeError("socket not connected")
        if not line.endswith("\n"):
            line = line + "\n"
        if self.debug and self.logf:
            log_dbg(f">>> {line.strip()}", self.logf)
        self.sock.sendall(line.encode("utf-8", "strict"))

    def _recv_line(self, timeout: float = 10.0) -> str:
        if not self.sock:
            raise RuntimeError("socket not connected")
        self.sock.settimeout(timeout)
        chunks: List[bytes] = []
        while True:
            b = self.sock.recv(1)
            if not b:
                break
            if b == b"\n":
                break
            chunks.append(b)
        line = b"".join(chunks).decode("utf-8", "replace")
        if self.debug and self.logf:
            log_dbg(f"<<< {line}", self.logf)
        if self.pretty and self.logf:
            try:
                parsed = json.loads(line)
                self.logf.write(json.dumps(parsed, indent=2, ensure_ascii=False) + "\n")
                self.logf.flush()
            except Exception:
                pass
        return line

    def call(self, method: str, params: Optional[dict] = None) -> Dict[str, Any]:
        req = {"jsonrpc": "2.0", "id": self.rpc_id, "method": method}
        self.rpc_id += 1
        if params is not None:
            req["params"] = params
        line = json.dumps(req, separators=(",", ":"))

        for attempt in (1, 2):
            try:
                if self.sock is None:
                    self.connect()
                self._send_line(line)
                resp_line = self._recv_line(timeout=10.0)
                if not resp_line:
                    raise RuntimeError("empty response (connection closed?)")
                return json.loads(resp_line)
            except Exception as e:
                self.close()
                if attempt == 2:
                    raise
                time.sleep(0.1)
        raise RuntimeError("RPC call failed after reconnect")

# ------------------------- Quiet, throttled progress -------------------------

class ProgressPrinter:
    """
    Prints a single updating progress line. Updates only if:
      - percent changed, or
      - state changed, or
      - >= throttle_sec elapsed since last print.
    Falls back to newline printing if not a TTY.
    """
    def __init__(self, quiet: bool, color: Color, throttle_ms: int = 1000):
        self.quiet = quiet
        self.color = color
        self.throttle_sec = max(0.05, throttle_ms / 1000.0)
        self._last_pct: Optional[int] = None
        self._last_state: Optional[str] = None
        self._last_t = 0.0
        self._is_tty = sys.stderr.isatty()

    def update(self, percent: Union[int, str], state: str, message: str):
        if self.quiet:
            return
        try:
            pct = int(percent)
        except Exception:
            pct = 0

        now = time.time()
        need = (pct != self._last_pct) or (state != self._last_state) or ((now - self._last_t) >= self.throttle_sec)

        if not need:
            return

        self._last_pct = pct
        self._last_state = state
        self._last_t = now

        msg = f"{pct:3d}% {message}" if message else f"{pct:3d}%"
        line = f"[{ts()}] {self.color.B}{msg}{self.color.N}"

        if self._is_tty:
            sys.stderr.write("\r" + " " * 100 + "\r")  # clear line
            sys.stderr.write(line)
            sys.stderr.flush()
        else:
            sys.stderr.write(line + "\n")
            sys.stderr.flush()

    def finish(self, ok: bool, tail: str = ""):
        if self.quiet:
            return
        if sys.stderr.isatty():
            sys.stderr.write("\r" + " " * 100 + "\r")
        color = self.color.G if ok else self.color.R
        sys.stderr.write(f"{color}[{ts()}] {'OK' if ok else 'ERROR'}{self.color.N} {tail}\n")
        sys.stderr.flush()

# -------------------------------- Main ---------------------------------------

def main():
    a = parse_args()
    color = Color(enabled=not a.no_color)
    logf = open_logfile(a.log_file)

    def info(msg: str):
        log_line(msg, a.quiet)

    # Build params for importAs
    import_params = {
        "path": a.file,
        "name": a.name,
        "validateDetect": bool(a.validate_detect),
        "rpmMin": int(a.rpm_min),
        "timeoutMs": int(a.timeout_ms),
    }

    cli = RpcClient(a.host, a.port, debug=a.debug, pretty=a.pretty_json, logf=logf)
    cli.set_start_id(a.id_start)

    # Connect
    try:
        cli.connect()
    except Exception as e:
        info(f"{color.R}Failed to connect: {e}{color.N}")
        sys.exit(10)

    # Start import
    info(f"{color.C}[{ts()}] Starting import{color.N}: path={a.file}, name={a.name}")
    try:
        resp_start = cli.call("profile.importAs", import_params)
    except Exception as e:
        info(f"{color.R}Error: profile.importAs failed: {e}{color.N}")
        sys.exit(20)

    job_id = try_paths(resp_start, ["result.data.jobId", "result.jobId"])
    if not job_id:
        info(f"{color.R}Error: jobId missing in response of profile.importAs{color.N}")
        sys.exit(21)

    # Poll
    pp = ProgressPrinter(quiet=a.quiet, color=color, throttle_ms=max(100, a.progress_every_ms))
    state = "running"

    interval = max(10, a.interval_ms) / 1000.0
    ok = False
    while True:
        try:
            resp_stat = cli.call("profile.importStatus", {"jobId": job_id})
        except Exception as e:
            info(f"{color.R}Error: profile.importStatus failed: {e}{color.N}")
            pp.finish(False, "status failed")
            sys.exit(22)

        state = (try_paths(resp_stat, ["result.data.state", "result.state"]) or "running").lower()
        progress = try_paths(resp_stat, ["result.data.progress", "result.progress"]) or 0
        message = try_paths(resp_stat, ["result.data.message", "result.message", "result.msg"]) or ""
        error_msg = try_paths(resp_stat, ["result.data.error", "result.error", "error.message"]) or ""

        pp.update(progress, state, message)

        if state in ("done", "finished", "success"):
            ok = True
            break
        if state in ("error", "failed"):
            ok = False
            if not a.quiet:
                # kurze, klare Fehlermeldung
                sys.stderr.write(f"\n{color.R}Import error:{color.N} {error_msg or message}\n")
            pp.finish(False, error_msg or message)
            sys.exit(23)

        time.sleep(interval)

    pp.finish(True, "import finished")

    # Commit?
    if a.commit:
        info(f"{color.C}[{ts()}] Committing profile...{color.N}")
        try:
            _ = cli.call("profile.importCommit", {"jobId": job_id})
        except Exception as e:
            info(f"{color.R}Error: profile.importCommit failed: {e}{color.N}")
            sys.exit(24)
        info(f"{color.G}[{ts()}] Commit done.{color.N}")

        if a.enable_engine:
            try:
                _ = cli.call("engine.enable", None)
            except Exception as e:
                info(f"{color.R}Error: engine.enable failed: {e}{color.N}")
                sys.exit(25)
            info(f"{color.G}[{ts()}] Engine enabled.{color.N}")

    try:
        cli.close()
    except Exception:
        pass

    # final OK (nichts weiter spammen)
    sys.exit(0)

if __name__ == "__main__":
    main()

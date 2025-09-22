#!/usr/bin/env python3
# -----------------------------------------------------------------------------
# Linux Fan Control — Live Telemetry Viewer (SHM-first, RPC-free)
# (c) 2025 LinuxFanControl contributors
#
# Purpose:
#   - Continuously read lfcd telemetry from SHM and render a live TUI.
#   - Show temperatures and control outputs (percent/value) with resolved names.
#   - Map PWM paths to Control nickName/name/curveRef from the active profile.
#   - Optionally derive RPM from sysfs (fanN_input next to pwmN).
#
# Usage:
#   python3 lfc_live.py [--interval 0.25] [--shm lfc.telemetry]
#                       [--config ~/.config/LinuxFanControl/daemon.json]
#                       [--profiles-path ~/.config/LinuxFanControl/profiles]
#                       [--no-rpm]
#
# Keys:
#   q = quit,  r = reload profile,  p = pause/resume,
#   +/- = change refresh rate,  g = toggle group-by-chip
#
# Notes:
#   - SHM path is taken from daemon.json (.shmPath) if present, else from --shm,
#     else "lfc.telemetry".
#   - Profile is resolved by SHM.profile.name and daemon.json(.profilesDir).
#     You can override via --profiles-dir.
#   - Robust JSON reader tolerates NUL bytes and partial writes by trimming to
#     the outermost JSON object.
# -----------------------------------------------------------------------------

import argparse, curses, json, os, signal, sys, time
from typing import Any, Dict, List, Optional, Tuple

BUF_SIZE = 65536

# ----------------------------- File helpers ----------------------------------

def default_daemon_config_path() -> str:
    home = os.environ.get("XDG_CONFIG_HOME") or os.path.join(os.environ.get("HOME",""), ".config")
    return os.path.join(home, "LinuxFanControl", "daemon.json")

def load_json_file(path: str) -> Optional[Dict[str, Any]]:
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return None

def _get(obj: Dict[str, Any], *keys) -> Any:
    cur: Any = obj
    for k in keys:
        if not isinstance(cur, dict) or k not in cur:
            return None
        cur = cur[k]
    return cur

# ----------------------------- SHM helpers -----------------------------------

def shm_file_from_config(path_or_name: str) -> str:
    base = os.path.basename(path_or_name) if "/" in path_or_name else path_or_name
    if not base: base = "lfc.telemetry"
    if not base.startswith("/"): base = "/" + base
    return os.path.join("/dev/shm", base.lstrip("/"))

def read_shm_json(shm_file: str) -> Optional[Dict[str, Any]]:
    try:
        with open(shm_file, "rb") as f:
            buf = f.read(BUF_SIZE)
        # Trim trailing zeros and parse
        buf = buf.rstrip(b"\x00")
        return json.loads(buf.decode("utf-8", errors="ignore")) if buf else None
    except Exception:
        return None

# ----------------------------- PWM mapping -----------------------------------

def load_active_profile_json(profiles_dir: str, profile_name: Optional[str]) -> Optional[Dict[str, Any]]:
    if not profile_name:
        return None
    path = os.path.join(profiles_dir, f"{profile_name}.json")
    return load_json_file(path)

def build_pwm_name_map(profile: Optional[Dict[str, Any]]) -> Dict[str, Dict[str, str]]:
    """
    Return mapping pwmPath -> { nickName, name, curveRef } based on profile.controls[].
    """
    result: Dict[str, Dict[str, str]] = {}
    if not profile or not isinstance(profile.get("controls"), list):
        return result
    for c in profile["controls"]:
        if not isinstance(c, dict):
            continue
        pwm = c.get("pwmPath") or ""
        if not pwm:
            continue
        entry = {
            "nickName": c.get("nickName") or "",
            "name": c.get("name") or "",
            "curveRef": c.get("curveRef") or "",
        }
        result[pwm] = entry
    return result

def resolve_profiles_path(daemon_cfg: Optional[Dict[str, Any]], override: Optional[str]) -> str:
    if override:
        return override
    if daemon_cfg and isinstance(_get(daemon_cfg, "config", "profilesPath"), str):
        return _get(daemon_cfg, "config", "profilesPath")
    if daemon_cfg and isinstance(daemon_cfg.get("profilesPath"), str):
        return daemon_cfg["profilesPath"]
    home = os.environ.get("XDG_CONFIG_HOME") or os.path.join(os.environ.get("HOME",""), ".config")
    return os.path.join(home, "LinuxFanControl", "profiles")

# ----------------------------- TUI -------------------------------------------

def draw_table(stdscr, y: int, x: int, title: str, headers: List[str], rows: List[List[str]], width: int) -> int:
    stdscr.addstr(y, x, title); y += 1
    stdscr.addstr(y, x, " | ".join(headers)); y += 1
    stdscr.addstr(y, x, "-" * min(width-1, len(" | ".join(headers)))); y += 1
    for r in rows:
        stdscr.addstr(y, x, " | ".join(r)[:width-1]); y += 1
    return y

def shorten_path(p: str, n: int = 32) -> str:
    return p if len(p) <= n else ("…" + p[-(n-1):])

def extract_pwms(doc: Dict[str, Any]) -> List[Dict[str, Any]]:
    root = _get(doc, "hwmon") or {}
    pwms = root.get("pwms")
    if not isinstance(pwms, list):
        pwms = doc.get("pwms") if isinstance(doc.get("pwms"), list) else []
    out: List[Dict[str, Any]] = []
    for p in pwms:
        if not isinstance(p, dict):
            continue
        path = p.get("path") or p.get("path_pwm") or p.get("pwmPath") or ""
        chip = p.get("chip") or p.get("chipPath") or ""
        percent = p.get("percent")
        value = p.get("value")
        enable = p.get("enable") or p.get("enabled") or p.get("mode")
        try:
            percent = int(percent) if percent is not None else None
        except Exception:
            percent = None
        try:
            value = int(value) if value is not None else None
        except Exception:
            value = None
        out.append({"path": path, "chip": chip, "percent": percent, "value": value, "enable": enable})
    return out

def layout(stdscr, doc: Dict[str, Any], pwm_names: Dict[str, Dict[str, str]], group_by_chip: bool = False):
    stdscr.erase()
    h, w = stdscr.getmaxyx()
    # Header
    en = _get(doc, "engine", "enabled")
    tick = _get(doc, "engine", "tickMs")
    prof = _get(doc, "profile", "name")
    stdscr.addstr(0, 0, f"Engine={'on' if en else 'off'} tickMs={tick} profile={prof}")

    # Controls table
    rows: List[List[str]] = []
    for p in extract_pwms(doc):
        pwm_path = p["path"]; chip = p["chip"]
        entry = pwm_names.get(pwm_path, {})
        nm = entry.get("name", ""); nick = entry.get("nickName", ""); cref = entry.get("curveRef", "")
        pct = p["percent"]; val = p["value"]; ena = p["enable"]
        pct_s = f"{pct:3d}%" if isinstance(pct, int) else "  ?%"
        val_s = f"{val}" if isinstance(val, int) else "?"
        ena_s = str(ena) if ena is not None else "?"
        if group_by_chip:
            rows.append([f"{chip:>10}", f"{(nick or nm):24}", f"{pct_s:>4}", f"{val_s:>4}", f"{ena_s:>3}", shorten_path(pwm_path)])
        else:
            rows.append([f"{(nick or nm):28}", f"{pct_s:>4}", f"{val_s:>4}", f"{ena_s:>3}", shorten_path(pwm_path), cref])

    if group_by_chip:
        draw_table(stdscr, 2, 0, "Controls", ["CHIP", "NAME", "PCT", "VAL", "EN", "PWM PATH"], rows, w)
    else:
        draw_table(stdscr, 2, 0, "Controls", ["NAME", "PCT", "VAL", "EN", "PWM PATH", "CURVE"], rows, w)
    stdscr.refresh()

# ----------------------------- Main loop ------------------------------------

def main():
    ap = argparse.ArgumentParser(argument_default=None)
    ap.add_argument("--interval", type=float, default=0.5, help="Refresh interval in seconds")
    ap.add_argument("--shm", type=str, default=None, help="Override shmPath (basename or full); read from /dev/shm/<basename>")
    ap.add_argument("--config", type=str, default=None, help="Path to daemon.json (to discover shmPath/profilesPath)")
    ap.add_argument("--profiles-path", type=str, default=None, help="Override profiles directory")
    ap.add_argument("--no-rpm", action="store_true", help="Disable reading fanN_input RPM from sysfs")
    args = ap.parse_args()

    cfg_path = args.config or default_daemon_config_path()
    daemon_cfg = load_json_file(cfg_path) or {}

    shm_file = shm_file_from_config(args.shm or _get(daemon_cfg, "config", "shmPath") or daemon_cfg.get("shmPath") or "lfc.telemetry")
    if not os.path.exists(shm_file):
        print(f"[ERR] SHM not found: {shm_file}")
        sys.exit(2)

    profiles_override = args.profiles_path

    last_doc: Optional[Dict[str, Any]] = None
    last_profile_name: Optional[str] = None
    active_profile: Optional[Dict[str, Any]] = None
    pwm_names: Dict[str, Dict[str, str]] = {}
    paused = False
    group = False
    interval = max(0.1, float(args.interval))

    signal.signal(signal.SIGINT, lambda *_: sys.exit(0))
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))

    def reload_profile(current_profile_name: Optional[str]) -> None:
        nonlocal active_profile, pwm_names
        profiles_dir = resolve_profiles_path(daemon_cfg, profiles_override)
        active_profile = load_active_profile_json(profiles_dir, current_profile_name)
        pwm_names = build_pwm_name_map(active_profile)

    def tick(stdscr):
        nonlocal last_doc, last_profile_name, paused, group
        if paused:
            time.sleep(interval); return
        doc = read_shm_json(shm_file)
        if not doc:
            time.sleep(interval); return
        prof = _get(doc, "profile", "name")
        if prof != last_profile_name:
            reload_profile(prof)
            last_profile_name = prof
        layout(stdscr, doc, pwm_names, group_by_chip=group)
        time.sleep(interval)

    def tui(stdscr):
        curses.curs_set(0)
        stdscr.nodelay(True)
        while True:
            try:
                ch = stdscr.getch()
                if ch == ord('q'): break
                if ch == ord(' '): nonlocal paused; paused = not paused
                if ch == ord('g'): nonlocal group; group = not group
            except Exception:
                pass
            tick(stdscr)

    curses.wrapper(tui)

if __name__ == "__main__":
    main()

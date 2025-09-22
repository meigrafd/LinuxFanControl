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
#                       [--profiles-dir ~/.config/LinuxFanControl/profiles]
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

import argparse
import curses
import json
import os
import signal
import sys
import time
from typing import Any, Dict, List, Optional, Tuple


# ----------------------------- Config helpers -------------------------------

def xdg_config_home() -> str:
    return os.environ.get("XDG_CONFIG_HOME", os.path.expanduser("~/.config"))


def default_daemon_config_path() -> str:
    return os.path.join(xdg_config_home(), "LinuxFanControl", "daemon.json")


def load_json_file(path: str) -> Optional[Dict[str, Any]]:
    try:
        with open(path, "rb") as f:
            data = f.read()
        # Accept files with potential NULs; decode as UTF-8 replace
        text = data.replace(b"\x00", b"").decode("utf-8", "replace")
        return json.loads(text)
    except Exception:
        return None


# ----------------------------- SHM utilities --------------------------------

def shm_file_from_config(shm_path_cfg: Optional[str]) -> str:
    # Accept "lfc.telemetry" or "/lfc.telemetry" from config; always read /dev/shm/<basename>
    base = (shm_path_cfg or "lfc.telemetry").split("/")[-1] or "lfc.telemetry"
    return f"/dev/shm/{base}"


def read_shm_json(shm_file: str, last_good: Optional[Dict[str, Any]]) -> Optional[Dict[str, Any]]:
    """
    Read JSON from SHM robustly:
      - Strip NUL bytes
      - Try full parse; if fails, trim to first '{' ... last '}' and parse
      - If still fails, return last_good (no update) to avoid flicker
    """
    try:
        with open(shm_file, "rb") as f:
            raw = f.read().replace(b"\x00", b"")
    except Exception:
        return last_good
    if not raw:
        return last_good
    text = raw.decode("utf-8", "replace")
    # fast path
    try:
        return json.loads(text)
    except Exception:
        pass
    # trimmed fragment
    i = text.find("{")
    j = text.rfind("}")
    if i != -1 and j != -1 and j > i:
        frag = text[i:j+1]
        try:
            return json.loads(frag)
        except Exception:
            return last_good
    return last_good


# ----------------------------- Telemetry parsing ----------------------------

def _get(d: Dict[str, Any], *path: str, default=None):
    cur: Any = d
    for p in path:
        if not isinstance(cur, dict) or p not in cur:
            return default
        cur = cur[p]
    return cur


def extract_engine(doc: Dict[str, Any]) -> Tuple[bool, Optional[str], Optional[int]]:
    enabled = bool(doc.get("engineEnabled", False))
    prof = _get(doc, "profile", "name")
    tick = _get(doc, "engine", "tickMs")
    if tick is None:
        tick = doc.get("tickMs")
    return enabled, prof, tick


def extract_temps(doc: Dict[str, Any]) -> List[Dict[str, Any]]:
    root = _get(doc, "hwmon") or {}
    temps = root.get("temps")
    if not isinstance(temps, list):
        temps = doc.get("temps") if isinstance(doc.get("temps"), list) else []
    out: List[Dict[str, Any]] = []
    for t in temps:
        if not isinstance(t, dict):
            continue
        label = t.get("label") or t.get("name") or os.path.basename(str(t.get("path") or t.get("path_input") or "temp"))
        path = t.get("path") or t.get("path_input") or ""
        chip = t.get("chip") or t.get("chipPath") or ""
        valc = t.get("tempC") if isinstance(t.get("tempC"), (int, float)) else t.get("value")
        try:
            valc = float(valc)
        except Exception:
            valc = None
        out.append({"label": label, "path": path, "chip": chip, "tempC": valc})
    return out


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


# ----------------------------- Profile mapping ------------------------------

def resolve_profiles_dir(daemon_cfg: Optional[Dict[str, Any]], override: Optional[str]) -> str:
    if override:
        return override
    if daemon_cfg and isinstance(_get(daemon_cfg, "config", "profilesDir"), str):
        return _get(daemon_cfg, "config", "profilesDir")
    if daemon_cfg and isinstance(daemon_cfg.get("profilesDir"), str):
        return daemon_cfg["profilesDir"]
    return os.path.join(xdg_config_home(), "LinuxFanControl", "profiles")


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


# ----------------------------- Sysfs helpers --------------------------------

def rpm_path_for_pwm(pwm_path: str) -> Optional[str]:
    """
    Try to derive fanN_input path from pwmN path in the same directory.
    """
    try:
        bn = os.path.basename(pwm_path)  # e.g. pwm7
        if not bn.startswith("pwm"):
            return None
        idx = bn[3:]
        base = os.path.dirname(pwm_path)
        cand = os.path.join(base, f"fan{idx}_input")
        if os.path.exists(cand):
            return cand
        # Fallback: check a few common indices if index not numeric
        for i in range(1, 9):
            alt = os.path.join(base, f"fan{i}_input")
            if os.path.exists(alt):
                return alt
    except Exception:
        pass
    return None


def read_int(path: str) -> Optional[int]:
    try:
        with open(path, "r") as f:
            s = f.read().strip()
        return int(s)
    except Exception:
        return None


# ----------------------------- TUI rendering --------------------------------

def shorten_path(path: str, keep: int = 2) -> str:
    if not path:
        return ""
    parts = path.strip("/").split("/")
    if len(parts) <= keep:
        return path
    return "…/" + "/".join(parts[-keep:])


def draw_table(stdscr, y: int, x: int, title: str, headers: List[str], rows: List[List[str]], maxw: int) -> int:
    stdscr.addstr(y, x, title[:maxw])
    y += 1
    # header
    header_line = "  ".join(headers)
    stdscr.addstr(y, x, header_line[:maxw])
    y += 1
    # rows
    for r in rows:
        line = "  ".join(r)
        stdscr.addstr(y, x, line[:maxw])
        y += 1
    return y


def render(stdscr, state: Dict[str, Any]) -> None:
    stdscr.erase()
    h, w = stdscr.getmaxyx()

    enabled = state.get("enabled")
    prof = state.get("profile")
    tick = state.get("tick")
    ts = time.strftime("%H:%M:%S")

    header = f"LFC Live — profile: {prof or '<none>'} | enabled: {enabled} | tick: {tick} ms | {ts}  (q=quit, r=reload, p=pause, +/-=rate, g=group)"
    stdscr.addstr(0, 0, header[:w])

    # Grouping toggle
    group_by_chip: bool = state.get("group", False)

    # Temperatures
    temps: List[Dict[str, Any]] = state.get("temps", [])
    if group_by_chip:
        temps_sorted = sorted(temps, key=lambda t: (t.get("chip", ""), t.get("label", "")))
    else:
        temps_sorted = sorted(temps, key=lambda t: t.get("label", ""))

    temp_rows: List[List[str]] = []
    for t in temps_sorted:
        lt = t.get("label") or ""
        chip = os.path.basename(t.get("chip") or "")
        pshrt = shorten_path(t.get("path") or "")
        val = t.get("tempC")
        sval = f"{val:5.1f}°C" if isinstance(val, (int, float)) else "  n/a"
        if group_by_chip:
            temp_rows.append([f"{chip:>10}", f"{lt:24}", f"{sval:>7}", pshrt])
        else:
            temp_rows.append([f"{lt:24}", f"{sval:>7}", pshrt])

    y = 2
    if group_by_chip:
        y = draw_table(stdscr, y, 0, "Temperatures", ["CHIP", "LABEL", "TEMP", "PATH"], temp_rows, w)
    else:
        y = draw_table(stdscr, y, 0, "Temperatures", ["LABEL", "TEMP", "PATH"], temp_rows, w)

    # Controls
    pwms: List[Dict[str, Any]] = state.get("pwms", [])
    name_map: Dict[str, Dict[str, str]] = state.get("pwm_names", {})
    rows: List[List[str]] = []

    if group_by_chip:
        pwms_sorted = sorted(pwms, key=lambda p: (p.get("chip", ""), p.get("path", "")))
    else:
        pwms_sorted = sorted(pwms, key=lambda p: p.get("path", ""))

    for p in pwms_sorted:
        pwm_path = p.get("path") or ""
        chip = os.path.basename(p.get("chip") or "")
        info = name_map.get(pwm_path, {})
        nick = info.get("nickName") or ""
        nm = info.get("name") or os.path.basename(pwm_path)
        cref = info.get("curveRef") or ""
        pct = p.get("percent")
        val = p.get("value")
        ena = p.get("enable")
        rpm = state.get("rpm_cache", {}).get(pwm_path)

        pct_s = f"{pct:3d}%" if isinstance(pct, int) else "  ?%"
        val_s = f"{val}" if isinstance(val, int) else "?"
        rpm_s = f"{rpm} rpm" if isinstance(rpm, int) else "—"
        ena_s = str(ena) if ena is not None else "?"

        if group_by_chip:
            rows.append([f"{chip:>10}", f"{(nick or nm):24}", f"{pct_s:>4}", f"{val_s:>4}", f"{rpm_s:>8}", f"{ena_s:>3}", shorten_path(pwm_path)])
        else:
            rows.append([f"{(nick or nm):28}", f"{pct_s:>4}", f"{val_s:>4}", f"{rpm_s:>8}", f"{ena_s:>3}", shorten_path(pwm_path), cref])

    if group_by_chip:
        draw_table(stdscr, y, 0, "Controls", ["CHIP", "NAME", "PCT", "VAL", "RPM", "EN", "PWM PATH"], rows, w)
    else:
        draw_table(stdscr, y, 0, "Controls", ["NAME", "PCT", "VAL", "RPM", "EN", "PWM PATH", "CURVE"], rows, w)

    stdscr.refresh()


# ----------------------------- Main loop ------------------------------------

def main():
    ap = argparse.ArgumentParser(argument_default=None)
    ap.add_argument("--interval", type=float, default=0.5, help="Refresh interval in seconds")
    ap.add_argument("--shm", type=str, default=None, help="Override shmPath (basename or full); read from /dev/shm/<basename>")
    ap.add_argument("--config", type=str, default=None, help="Path to daemon.json (to discover shmPath/profilesDir)")
    ap.add_argument("--profiles-dir", type=str, default=None, help="Override profiles directory")
    ap.add_argument("--no-rpm", action="store_true", help="Disable reading fanN_input RPM from sysfs")
    args = ap.parse_args()

    # Resolve daemon config
    cfg_path = args.config or default_daemon_config_path()
    daemon_cfg = load_json_file(cfg_path) or {}

    # Resolve SHM file
    shm_file = shm_file_from_config(args.shm or daemon_cfg.get("shmPath") or _get(daemon_cfg, "config", "shmPath"))
    if not os.path.exists(shm_file):
        print(f"[ERR] SHM not found: {shm_file}")
        sys.exit(2)

    # Profiles dir override is used when loading/refreshing active profile mapping
    profiles_dir_override = args.profiles_dir

    # State
    last_doc: Optional[Dict[str, Any]] = None
    last_profile_name: Optional[str] = None
    active_profile: Optional[Dict[str, Any]] = None
    pwm_names: Dict[str, Dict[str, str]] = {}
    rpm_cache: Dict[str, int] = {}
    paused = False
    group = False
    interval = max(0.1, float(args.interval))

    # Graceful exit
    signal.signal(signal.SIGINT, lambda *_: sys.exit(0))
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))

    def resolve_profiles_dir_local() -> str:
        return resolve_profiles_dir(daemon_cfg, profiles_dir_override)

    def reload_profile(current_profile_name: Optional[str]) -> None:
        nonlocal active_profile, pwm_names
        profiles_dir = resolve_profiles_dir_local()
        active_profile = load_active_profile_json(profiles_dir, current_profile_name)
        pwm_names = build_pwm_name_map(active_profile)

    def poll_once():
        nonlocal last_doc, last_profile_name, rpm_cache
        doc = read_shm_json(shm_file, last_doc)
        if doc is None:
            return None
        last_doc = doc

        enabled, prof, tick = extract_engine(doc)

        # (Re)load profile mapping if changed
        if prof and prof != last_profile_name:
            reload_profile(prof)
            last_profile_name = prof

        temps = extract_temps(doc)
        pwms = extract_pwms(doc)

        # RPM cache (best effort)
        for p in pwms:
            pwm_path = p.get("path") or ""
            if not pwm_path:
                continue
            rpath = rpm_path_for_pwm(pwm_path)
            if rpath:
                rpm_val = read_int(rpath)
                if rpm_val is not None:
                    rpm_cache[pwm_path] = rpm_val

        return {
            "enabled": enabled,
            "profile": prof,
            "tick": tick,
            "temps": temps,
            "pwms": pwms,
            "pwm_names": pwm_names,
            "rpm_cache": rpm_cache,
            "group": group,
        }

    def loop(stdscr):
        nonlocal paused, interval, group
        try:
            curses.curs_set(0)
        except Exception:
            pass
        stdscr.nodelay(True)
        last_render = 0.0
        while True:
            ch = stdscr.getch()
            if ch != -1:
                if ch in (ord('q'), ord('Q')):
                    break
                elif ch in (ord('r'), ord('R')):
                    reload_profile(last_profile_name)
                elif ch in (ord('p'), ord('P')):
                    paused = not paused
                elif ch == ord('+'):
                    interval = max(0.05, interval * 0.8)
                elif ch == ord('-'):
                    interval = min(5.0, interval * 1.25)
                elif ch in (ord('g'), ord('G')):
                    group = not group

            now = time.time()
            if not paused and (now - last_render) >= interval:
                state = poll_once()
                if state is not None:
                    render(stdscr, state)
                    last_render = now
            time.sleep(0.01)

    curses.wrapper(loop)


if __name__ == "__main__":
    main()

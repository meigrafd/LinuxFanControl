#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Linux Fan Control — live telemetry viewer (new SHM schema only)

Reads JSON that lfcd publishes to POSIX shm (/dev/shm/<name>), and prints
a compact human-readable snapshot. No legacy schema support.

(c) 2025 LinuxFanControl contributors
"""

import argparse
import json
import os
import shutil
import signal
import sys
import time
from typing import Any, Dict, List, Optional, Tuple

# ----------------------------- SHM I/O ---------------------------------------

def shm_file_from_name(name: str) -> str:
    # accept "lfc.telemetry" or "/lfc.telemetry"
    base = name[1:] if name.startswith("/") else name
    return os.path.join("/dev/shm", base)

def read_shm_json(shm_file: str) -> Optional[Dict[str, Any]]:
    try:
        with open(shm_file, "rb") as f:
            data = f.read()
        if not data:
            return None
        return json.loads(data.decode("utf-8", errors="ignore"))
    except Exception:
        return None

# ----------------------------- formatting ------------------------------------

def tcols(default: int = 120) -> int:
    try:
        return shutil.get_terminal_size((default, 24)).columns
    except Exception:
        return default

def ell(s: str, w: int) -> str:
    s = s or ""
    return s if len(s) <= w else (s[: max(1, w - 1)] + "…")

def fmt_i(x: Optional[int]) -> str:
    return "-" if x is None else str(x)

def fmt_f(x: Optional[float], digits: int = 1) -> str:
    if x is None:
        return "-"
    try:
        return f"{float(x):.{digits}f}"
    except Exception:
        return "-"

def mode_from_enable(en: Optional[int], has_path: bool) -> str:
    if not has_path:
        return "N/A"
    if en is None:
        return "—"
    if en == 0:
        return "AUTO"
    if en in (1, 2):
        return "MAN"
    if en in (3, 4, 5):
        return "HW"
    return str(en)

def rule(widths: List[int]) -> str:
    return "  " + "-+-".join("-" * w for w in widths)

# ----------------------------- extractors ------------------------------------

def extract_header(doc: Dict[str, Any]) -> Tuple[str, bool]:
    return str(doc.get("version", "")), bool(doc.get("engineEnabled", False))

def extract_profile(doc: Dict[str, Any]) -> Dict[str, Any]:
    p = doc.get("profile") or {}
    return {
        "name": p.get("name") or "",
        "controlCount": p.get("controlCount"),
        "curveCount": p.get("curveCount"),
    }

def extract_chips(doc: Dict[str, Any]) -> List[Dict[str, Any]]:
    return [c for c in (doc.get("chips") or []) if isinstance(c, dict)]

def extract_temps(doc: Dict[str, Any]) -> List[Dict[str, Any]]:
    out = []
    for t in (doc.get("temps") or []):
        if not isinstance(t, dict):
            continue
        out.append({
            "chipPath": t.get("chipPath") or "",
            "inputPath": t.get("inputPath") or "",
            "label": t.get("label") or "",
            "valueC": t.get("valueC"),
        })
    return out

def extract_fans(doc: Dict[str, Any]) -> List[Dict[str, Any]]:
    out = []
    for f in (doc.get("fans") or []):
        if not isinstance(f, dict):
            continue
        out.append({
            "chipPath": f.get("chipPath") or "",
            "inputPath": f.get("inputPath") or "",
            "label": f.get("label") or "",
            "rpm": f.get("rpm"),
        })
    return out

def extract_pwms(doc: Dict[str, Any]) -> List[Dict[str, Any]]:
    out = []
    for p in (doc.get("pwms") or []):
        if not isinstance(p, dict):
            continue
        out.append({
            "chipPath":   p.get("chipPath") or "",
            "pwmPath":    p.get("pwmPath") or "",
            "enablePath": p.get("enablePath") or "",
            "pwmMax":     p.get("pwmMax"),
            "enable":     p.get("enable"),
            "raw":        p.get("raw"),
            "percent":    p.get("percent"),
            "fanRpm":     p.get("fanRpm"),
            "label":      p.get("label") or "",
        })
    return out

def extract_gpus(doc: Dict[str, Any]) -> List[Dict[str, Any]]:
    return [g for g in (doc.get("gpus") or []) if isinstance(g, dict)]

# ----------------------------- rendering -------------------------------------

def render_header(doc: Dict[str, Any]) -> None:
    cols = tcols()
    version, enabled = extract_header(doc)
    prof = extract_profile(doc)
    print("\n" + "=" * cols)
    print(f"LinuxFanControl Telemetry  |  version={version}  |  engine={'ENABLED' if enabled else 'disabled'}")
    if prof["name"]:
        print(f"Active profile: {prof['name']} "
              f"(controls={prof['controlCount']}, curves={prof['curveCount']})")
    print("-" * cols)

def render_gpus(doc: Dict[str, Any]) -> None:
    rows = extract_gpus(doc)
    if not rows:
        return
    W_VEND, W_IDX, W_PCI, W_DRM, W_CAP, W_RPM, W_T = 8, 3, 12, 12, 7, 7, 8
    print("GPUs:")
    print("  " + f"{'Vendor':<{W_VEND}} | {'#':>{W_IDX}} | {'PCI':<{W_PCI}} | {'DRM':<{W_DRM}} | "
          f"{'Fan':<{W_CAP}} | {'RPM':>{W_RPM}} | {'Edge °C':>{W_T}} | {'Hotspot °C':>{W_T}} | {'Mem °C':>{W_T}} | Hwmon")
    print(rule([W_VEND, W_IDX, W_PCI, W_DRM, W_CAP, W_RPM, W_T, W_T, W_T, 10]))
    for g in rows:
        vend = (g.get("vendor") or "")[:W_VEND]
        idx  = g.get("index")
        pci  = g.get("pci") or ""
        drm  = g.get("drm") or ""
        cap  = ("tach" if g.get("hasFanTach") else "-") + "/" + ("pwm" if g.get("hasFanPwm") else "-")
        rpm  = g.get("fanRpm")
        tE   = g.get("tempEdgeC")
        tH   = g.get("tempHotspotC")
        tM   = g.get("tempMemoryC")
        hw   = g.get("hwmon") or ""
        print("  " + f"{vend:<{W_VEND}} | {idx:>{W_IDX}} | {ell(pci, W_PCI):<{W_PCI}} | "
              f"{ell(drm, W_DRM):<{W_DRM}} | {cap:<{W_CAP}} | {fmt_i(rpm):>{W_RPM}} | "
              f"{fmt_f(tE):>{W_T}} | {fmt_f(tH):>{W_T}} | {fmt_f(tM):>{W_T}} | {hw}")

def render_chips(doc: Dict[str, Any]) -> None:
    chips = extract_chips(doc)
    if not chips:
        return
    print("\nHWMON Chips:")
    for c in chips:
        name = c.get("name") or ""
        vendor = c.get("vendor") or ""
        path = c.get("path") or ""
        print(f"  {name:<12}  {vendor:<14}  {path}")

def render_temps(doc: Dict[str, Any]) -> None:
    rows = extract_temps(doc)
    if not rows:
        print("\nTemperatures: none")
        return
    W_CHIP, W_LBL, W_VAL = 14, 24, 8
    print("\nTemperatures:")
    print("  " + f"{'Chip':<{W_CHIP}} | {'Label':<{W_LBL}} | {'Value °C':>{W_VAL}} | Path")
    print(rule([W_CHIP, W_LBL, W_VAL, 10]))
    for t in rows:
        chip = os.path.basename(t["chipPath"]) or t["chipPath"]
        lbl  = t["label"] or os.path.basename(t["inputPath"])
        print("  " + f"{ell(chip, W_CHIP):<{W_CHIP}} | {ell(lbl, W_LBL):<{W_LBL}} | "
              f"{fmt_f(t.get('valueC')):>{W_VAL}} | {t['inputPath']}")

def render_fans(doc: Dict[str, Any]) -> None:
    rows = extract_fans(doc)
    if not rows:
        print("\nFans: none")
        return
    W_CHIP, W_LBL, W_RPM = 14, 24, 7
    print("\nFans (tach):")
    print("  " + f"{'Chip':<{W_CHIP}} | {'Label':<{W_LBL}} | {'RPM':>{W_RPM}} | Path")
    print(rule([W_CHIP, W_LBL, W_RPM, 10]))
    for f in rows:
        chip = os.path.basename(f["chipPath"]) or f["chipPath"]
        lbl  = f["label"] or os.path.basename(f["inputPath"])
        print("  " + f"{ell(chip, W_CHIP):<{W_CHIP}} | {ell(lbl, W_LBL):<{W_LBL}} | "
              f"{fmt_i(f.get('rpm')):>{W_RPM}} | {f['inputPath']}")

def render_pwms(doc: Dict[str, Any]) -> None:
    rows = extract_pwms(doc)
    if not rows:
        print("\nPWMs: none")
        return
    W_CHIP, W_LABEL, W_PCT, W_VAL, W_MODE, W_RPM = 14, 22, 7, 13, 6, 7
    print("\nPWMs:")
    print("  " + f"{'Chip':<{W_CHIP}} | {'Label':<{W_LABEL}} | {'Percent':>{W_PCT}} | "
          f"{'Value/Max':<{W_VAL}} | {'Mode':<{W_MODE}} | {'RPM':>{W_RPM}} | PWM Path")
    print(rule([W_CHIP, W_LABEL, W_PCT, W_VAL, W_MODE, W_RPM, 10]))
    for r in rows:
        chip  = os.path.basename(r["chipPath"]) or r["chipPath"]
        label = r.get("label") or os.path.basename(r["pwmPath"])
        pct   = "-" if r.get("percent") is None else f"{int(r['percent'])}%"
        raw   = fmt_i(r.get("raw"))
        mxv   = r.get("pwmMax")
        vmax  = "-" if mxv is None else str(mxv)
        v_m   = f"{raw}/{vmax}" if vmax != "-" else raw
        mode  = mode_from_enable(r.get("enable"), bool(r.get("enablePath")))
        rpm   = fmt_i(r.get("fanRpm"))
        print("  " + f"{ell(chip, W_CHIP):<{W_CHIP}} | {ell(label, W_LABEL):<{W_LABEL}} | "
              f"{pct:>{W_PCT}} | {v_m:<{W_VAL}} | {mode:<{W_MODE}} | {rpm:>{W_RPM}} | {r['pwmPath']}")

# ----------------------------- main ------------------------------------------

def main() -> int:
    ap = argparse.ArgumentParser(description="Linux Fan Control — live telemetry (new SHM schema)")
    ap.add_argument("--shm", default="lfc.telemetry",
                    help="POSIX shm name (e.g. lfc.telemetry or /lfc.telemetry)")
    ap.add_argument("--json", action="store_true", help="Dump JSON and exit")
    ap.add_argument("--interval", type=float, default=1.0,
                    help="Refresh interval seconds (default: 1.0)")
    ap.add_argument("--once", action="store_true", help="Print once and exit")
    args = ap.parse_args()

    shm_file = shm_file_from_name(args.shm)
    if not os.path.exists(shm_file):
        print(f"[!] SHM file not found: {shm_file}", file=sys.stderr)
        print("    Run lfcd first or pass --shm if you use a custom name.", file=sys.stderr)
        return 2

    stopping = {"flag": False}
    def _sig(*_a): stopping["flag"] = True
    signal.signal(signal.SIGINT, _sig)
    signal.signal(signal.SIGTERM, _sig)

    def tick() -> bool:
        doc = read_shm_json(shm_file)
        if not isinstance(doc, dict):
            print("[!] Could not parse telemetry JSON", file=sys.stderr)
            return False
        if args.json:
            print(json.dumps(doc, indent=2, ensure_ascii=False))
            return True
        try:
            os.system("clear")
        except Exception:
            pass
        render_header(doc)
        render_gpus(doc)
        render_chips(doc)
        render_temps(doc)
        render_fans(doc)
        render_pwms(doc)
        sys.stdout.flush()
        return True

    if args.once:
        ok = tick()
        return 0 if ok else 3

    iv = max(0.1, float(args.interval))
    last = 0.0
    while not stopping["flag"]:
        now = time.time()
        if now - last >= iv:
            tick()
            last = now
        time.sleep(0.05)

    return 0

if __name__ == "__main__":
    sys.exit(main())

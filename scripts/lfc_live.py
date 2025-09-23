#!/usr/bin/env python3
# Linux Fan Control — live telemetry viewer (SHM + sysfs reads for live values)
# All comments in English; no placeholders.

import argparse
import json
import os
import shutil
import signal
import sys
import time
from typing import Any, Dict, List, Optional, Tuple

# ----------------------------- SHM helpers -----------------------------------

def shm_name_normalize(path_or_name: str) -> str:
    """Normalize daemon shm name/path to a POSIX shm name starting with '/'."""
    if not path_or_name:
        return "/lfc.telemetry"
    base = os.path.basename(path_or_name) if "/" in path_or_name else path_or_name
    return base if base.startswith("/") else "/" + base

def shm_file_from_name(path_or_name: str) -> str:
    """Translate a POSIX shm name to its file under /dev/shm for read-only access."""
    name = shm_name_normalize(path_or_name)
    return os.path.join("/dev/shm", name.lstrip("/"))

def read_shm_json(shm_file: str) -> Optional[Dict[str, Any]]:
    """Read and parse a JSON blob from a SHM-backed file."""
    try:
        if not os.path.exists(shm_file):
            return None
        size = max(131072, os.path.getsize(shm_file) or 0)
        with open(shm_file, "rb") as f:
            buf = f.read(size).rstrip(b"\x00")
        if not buf:
            return None
        return json.loads(buf.decode("utf-8", errors="ignore"))
    except Exception:
        return None

# ----------------------------- sysfs helpers ---------------------------------

def read_int(path: str) -> Optional[int]:
    try:
        with open(path, "r") as f:
            s = f.read().strip()
        return int(s)
    except Exception:
        return None

def read_text(path: str) -> Optional[str]:
    try:
        with open(path, "r", encoding="utf-8", errors="ignore") as f:
            return f.read().strip()
    except Exception:
        return None

def read_temp_c_from_input(path: str) -> Optional[float]:
    """Read millidegree value from *_input and convert to Celsius."""
    v = read_int(path)
    if v is None:
        return None
    # Common kernel ABI is millidegree C
    if abs(v) > 2000:
        return round(v / 1000.0, 1)
    # Fallback: assume already in °C
    return float(v)

def rpm_path_for_pwm(pwm_path: str) -> Optional[str]:
    """Best-effort: derive fanN_input from a pwmN path in the same directory."""
    try:
        base = os.path.dirname(pwm_path)
        bn = os.path.basename(pwm_path)
        if not bn.startswith("pwm"):
            return None
        idx = bn[3:]
        cand = os.path.join(base, f"fan{idx}_input")
        if os.path.exists(cand):
            return cand
        # fallback scan (first available)
        for i in range(1, 10):
            alt = os.path.join(base, f"fan{i}_input")
            if os.path.exists(alt):
                return alt
    except Exception:
        pass
    return None

def label_from_sysfs_neighbors(base_dir: str, index: str, prefer_pwm: bool = True) -> Optional[str]:
    """Try pwmN_label and fanN_label."""
    if prefer_pwm:
        lp = os.path.join(base_dir, f"pwm{index}_label")
        s = read_text(lp)
        if s:
            return s
        lf = os.path.join(base_dir, f"fan{index}_label")
        s = read_text(lf)
        if s:
            return s
    else:
        lf = os.path.join(base_dir, f"fan{index}_label")
        s = read_text(lf)
        if s:
            return s
        lp = os.path.join(base_dir, f"pwm{index}_label")
        s = read_text(lp)
        if s:
            return s
    return None

# ----------------------------- extractors ------------------------------------

def extract_engine(doc: Dict[str, Any]) -> Tuple[bool, str]:
    enabled = bool(doc.get("engineEnabled", False))
    version = str(doc.get("version") or "")
    return enabled, version

def extract_profile(doc: Dict[str, Any]) -> Dict[str, Any]:
    prof = doc.get("profile") or {}
    return {
        "name": prof.get("name") or "",
        "schema": prof.get("schema"),
        "description": prof.get("description"),
        "curveCount": prof.get("curveCount"),
        "controlCount": prof.get("controlCount"),
    }

def extract_chips(doc: Dict[str, Any]) -> List[Dict[str, Any]]:
    root = doc.get("hwmon") or {}
    chips = root.get("chips") or []
    return [c for c in chips if isinstance(c, dict)]

def extract_temps(doc: Dict[str, Any]) -> List[Dict[str, Any]]:
    root = doc.get("hwmon") or {}
    temps = root.get("temps") or []
    out: List[Dict[str, Any]] = []
    for t in temps:
        if not isinstance(t, dict):
            continue
        out.append({
            "chipPath": t.get("chipPath") or "",
            "path": t.get("path") or t.get("path_input") or "",
            "label": t.get("label") or "",
        })
    return out

def extract_fans(doc: Dict[str, Any]) -> List[Dict[str, Any]]:
    root = doc.get("hwmon") or {}
    fans = root.get("fans") or []
    out: List[Dict[str, Any]] = []
    for f in fans:
        if not isinstance(f, dict):
            continue
        out.append({
            "chipPath": f.get("chipPath") or "",
            "path": f.get("path") or "",
            "label": f.get("label") or "",
        })
    return out

def extract_pwms(doc: Dict[str, Any]) -> List[Dict[str, Any]]:
    root = doc.get("hwmon") or {}
    pwms = root.get("pwms") or []
    out: List[Dict[str, Any]] = []
    for p in pwms:
        if not isinstance(p, dict):
            continue
        chip_path   = p.get("chipPath") or ""
        pwm_path    = p.get("path") or ""
        enable_path = p.get("pathEnable") or ""
        pwm_max     = p.get("pwmMax")
        label       = p.get("label") or ""

        # live reads
        value  = read_int(pwm_path) if pwm_path else None
        enable = read_int(enable_path) if enable_path else None
        try:
            mx = int(pwm_max) if pwm_max is not None else None
        except Exception:
            mx = None
        percent = None
        if value is not None and mx not in (None, 0):
            try:
                percent = int(round(value * 100.0 / mx))
            except Exception:
                percent = None

        rpm = None
        if pwm_path:
            base = os.path.dirname(pwm_path)
            bn = os.path.basename(pwm_path)
            if bn.startswith("pwm"):
                idx = bn[3:]
                cand = os.path.join(base, f"fan{idx}_input")
                if os.path.exists(cand):
                    rpm = read_int(cand)
                else:
                    rpm = read_int(rpm_path_for_pwm(pwm_path) or "")

        # display label: prefer telemetry; fallback to sysfs neighbors
        disp_label = label
        if (not disp_label) and pwm_path:
            base = os.path.dirname(pwm_path)
            bn = os.path.basename(pwm_path)
            if bn.startswith("pwm"):
                idx = bn[3:]
                got = label_from_sysfs_neighbors(base, idx, prefer_pwm=True)
                if got:
                    disp_label = got

        out.append({
            "chipPath": chip_path,
            "pwmPath": pwm_path,
            "enablePath": enable_path,
            "pwmMax": mx,
            "label": label,
            "displayLabel": disp_label or os.path.basename(pwm_path),
            "value": value,
            "percent": percent,
            "enable": enable,
            "rpm": rpm,
        })
    return out

def extract_gpus(doc: Dict[str, Any]) -> List[Dict[str, Any]]:
    gpus = doc.get("gpus") or []
    return [g for g in gpus if isinstance(g, dict)]

# ----------------------------- formatting ------------------------------------

def mode_str(enable: Optional[int], has_enable_path: bool) -> str:
    """
    Map raw enable values to readable mode:
      - 0 => AUTO
      - 1/2 => MAN
      - 3/4/5 => HW   (smart/hardware modes on some chipsets)
      - unknown int => str(value)
      - no enable path => 'N/A'
      - unreadable => '—'
    """
    if not has_enable_path:
        return "N/A"
    if enable is None:
        return "—"
    if enable == 0:
        return "AUTO"
    if enable in (1, 2):
        return "MAN"
    if enable in (3, 4, 5):
        return "HW"
    return str(enable)

def fmt_none(x: Optional[int]) -> str:
    return "-" if x is None else str(x)

def fmt_none_f(x: Optional[float]) -> str:
    return "-" if x is None else f"{x:.1f}"

def draw_rule(widths: List[int]) -> str:
    parts = []
    for w in widths:
        parts.append("-" * w)
    return "  " + "-+-".join(parts)

def ell(s: str, w: int) -> str:
    if len(s) <= w:
        return s
    return s[: w - 1] + "…"

# ----------------------------- renderers -------------------------------------

def print_header(doc: Dict[str, Any]) -> None:
    enabled, version = extract_engine(doc)
    prof = extract_profile(doc)
    cols = shutil.get_terminal_size((120, 20)).columns

    print("\n" + "=" * cols)
    print(f"LinuxFanControl Telemetry  |  version={version}  |  engine={'ENABLED' if enabled else 'disabled'}")
    if prof["name"]:
        extra = f"schema={prof['schema']}  curves={prof['curveCount']}  controls={prof['controlCount']}"
        print(f"Active profile: {prof['name']}  ({extra})")
    print("-" * cols)

def print_gpus(doc: Dict[str, Any]) -> None:
    rows = extract_gpus(doc)
    if not rows:
        return
    W_VEND = 8
    W_IDX  = 5
    W_PCI  = 12
    W_DRM  = 14
    W_CAP  = 7
    W_RPM  = 7
    W_T    = 8
    print("GPUs:")
    hdr = ["Vendor", "Idx", "PCI", "DRM", "Fan", "RPM", "Edge °C", "Hotspot °C", "Mem °C", "Hwmon"]
    print("  " + f"{hdr[0]:<{W_VEND}} | {hdr[1]:>{W_IDX}} | {hdr[2]:<{W_PCI}} | {hdr[3]:<{W_DRM}} | {hdr[4]:<{W_CAP}} | {hdr[5]:>{W_RPM}} | {hdr[6]:>{W_T}} | {hdr[7]:>{W_T}} | {hdr[8]:>{W_T}} | {hdr[9]}")
    print(draw_rule([W_VEND, W_IDX, W_PCI, W_DRM, W_CAP, W_RPM, W_T, W_T, W_T, 10]))
    for g in rows:
        vend = (g.get("vendor") or "")[:W_VEND]
        idx  = g.get("index")
        pci  = g.get("pci") or ""
        drm  = g.get("drm") or ""
        hasT = "tach" if g.get("hasFanTach") else "-"
        hasP = "pwm"  if g.get("hasFanPwm")  else "-"
        cap  = (hasT + "/" + hasP)
        rpm  = g.get("fanRpm")
        tE   = g.get("tempEdgeC")
        tH   = g.get("tempHotspotC")
        tM   = g.get("tempMemoryC")
        hw   = g.get("hwmon") or ""
        print("  " + f"{vend:<{W_VEND}} | {idx:>{W_IDX}} | {ell(pci, W_PCI):<{W_PCI}} | {ell(drm, W_DRM):<{W_DRM}} | {cap:<{W_CAP}} | {fmt_none(rpm):>{W_RPM}} | {fmt_none_f(tE):>{W_T}} | {fmt_none_f(tH):>{W_T}} | {fmt_none_f(tM):>{W_T}} | {hw}")

def print_chips(doc: Dict[str, Any]) -> None:
    chips = extract_chips(doc)
    if not chips:
        return
    print("\nHWMon Chips:")
    for c in chips:
        name = c.get("name") or ""
        vendor = c.get("vendor") or ""
        path = c.get("hwmonPath") or ""
        print(f"  {name:<12}  {vendor:<14}  {path}")

def print_temps(doc: Dict[str, Any]) -> None:
    rows = extract_temps(doc)
    if not rows:
        print("\nTemperatures: none")
        return
    W_CHIP = 14
    W_LBL  = 24
    W_VAL  = 8
    print("\nTemperatures:")
    hdr = ["Chip", "Label", "Value °C", "Path"]
    print("  " + f"{hdr[0]:<{W_CHIP}} | {hdr[1]:<{W_LBL}} | {hdr[2]:>{W_VAL}} | {hdr[3]}")
    print(draw_rule([W_CHIP, W_LBL, W_VAL, 10]))
    for t in rows:
        chip = os.path.basename(t["chipPath"]) or t["chipPath"]
        lbl  = t["label"] or os.path.basename(t["path"])
        val  = read_temp_c_from_input(t["path"])
        print("  " + f"{ell(chip, W_CHIP):<{W_CHIP}} | {ell(lbl, W_LBL):<{W_LBL}} | {fmt_none_f(val):>{W_VAL}} | {t['path']}")

def print_fans(doc: Dict[str, Any]) -> None:
    rows = extract_fans(doc)
    if not rows:
        print("\nFans: none")
        return
    W_CHIP = 14
    W_LBL  = 24
    W_RPM  = 7
    print("\nFans (tach):")
    hdr = ["Chip", "Label", "RPM", "Path"]
    print("  " + f"{hdr[0]:<{W_CHIP}} | {hdr[1]:<{W_LBL}} | {hdr[2]:>{W_RPM}} | {hdr[3]}")
    print(draw_rule([W_CHIP, W_LBL, W_RPM, 10]))
    for f in rows:
        chip = os.path.basename(f["chipPath"]) or f["chipPath"]
        lbl  = f["label"] or os.path.basename(f["path"])
        rpm  = read_int(f["path"])
        print("  " + f"{ell(chip, W_CHIP):<{W_CHIP}} | {ell(lbl, W_LBL):<{W_LBL}} | {fmt_none(rpm):>{W_RPM}} | {f['path']}")

def print_pwms(doc: Dict[str, Any]) -> None:
    rows = extract_pwms(doc)
    if not rows:
        print("\nPWMs: none")
        return
    W_CHIP   = 14
    W_LABEL  = 22
    W_PCT    = 7
    W_VAL    = 13
    W_MODE   = 6
    W_RPM    = 7
    print("\nPWMs:")
    hdr = ["Chip", "Label", "Percent", "Value/Max", "Mode", "RPM", "PWM Path"]
    print("  " + f"{hdr[0]:<{W_CHIP}} | {hdr[1]:<{W_LABEL}} | {hdr[2]:>{W_PCT}} | {hdr[3]:<{W_VAL}} | {hdr[4]:<{W_MODE}} | {hdr[5]:>{W_RPM}} | {hdr[6]}")
    print(draw_rule([W_CHIP, W_LABEL, W_PCT, W_VAL, W_MODE, W_RPM, 10]))
    for r in rows:
        chip  = os.path.basename(r["chipPath"]) or r["chipPath"]
        label = r["displayLabel"] or os.path.basename(r["pwmPath"])
        pct   = "-" if r["percent"] is None else f"{r['percent']}%"
        val   = fmt_none(r["value"])
        mx    = fmt_none(r["pwmMax"])
        v_m   = f"{val}/{mx}" if mx != "-" else val
        mode  = mode_str(r["enable"], bool(r["enablePath"]))
        rpm   = fmt_none(r["rpm"])
        print("  " + f"{ell(chip, W_CHIP):<{W_CHIP}} | {ell(label, W_LABEL):<{W_LABEL}} | {pct:>{W_PCT}} | {v_m:<{W_VAL}} | {mode:<{W_MODE}} | {rpm:>{W_RPM}} | {r['pwmPath']}")

# ----------------------------- main ------------------------------------------

def main() -> int:
    ap = argparse.ArgumentParser(description="Linux Fan Control — live telemetry viewer")
    ap.add_argument("--shm", default="lfc.telemetry",
                    help="POSIX shm name or path (default: lfc.telemetry)")
    ap.add_argument("--once", action="store_true", help="Print once and exit")
    ap.add_argument("--interval", type=float, default=1.0, help="Polling interval seconds (default 1.0)")
    ap.add_argument("--json", action="store_true", help="Dump raw JSON instead of pretty view")
    args = ap.parse_args()

    shm_file = shm_file_from_name(args.shm)

    # handle signals for clean exit
    stop = {"flag": False}
    def _sig(*_a): stop["flag"] = True
    signal.signal(signal.SIGINT, _sig)
    signal.signal(signal.SIGTERM, _sig)

    if not os.path.exists(shm_file):
        print(f"[!] Telemetry SHM file not found: {shm_file}", file=sys.stderr)
        print("    Hint: run lfcd or specify --shm if you use a custom name.", file=sys.stderr)
        return 2

    def tick() -> bool:
        doc = read_shm_json(shm_file)
        if not isinstance(doc, dict):
            print("[!] Could not parse telemetry JSON", file=sys.stderr)
            return False
        if args.json:
            print(json.dumps(doc, indent=2, ensure_ascii=False))
        else:
            try:
                os.system("clear")
            except Exception:
                pass
            print_header(doc)
            print_gpus(doc)
            print_chips(doc)
            print_temps(doc)
            print_fans(doc)
            print_pwms(doc)
        sys.stdout.flush()
        return True

    if args.once:
        ok = tick()
        return 0 if ok else 3

    last = 0.0
    iv = max(0.1, float(args.interval))
    while not stop["flag"]:
        now = time.time()
        if now - last >= iv:
            tick()
            last = now
        time.sleep(0.05)
    return 0

if __name__ == "__main__":
    sys.exit(main())

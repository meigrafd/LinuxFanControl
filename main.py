#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Linux Fan Control  (single-file)
- Auto detection: sensors & PWM via /sys/class/hwmon
- Safe active coupling via PWM perturbations (never below floor; snapshot & restore)
- Separate calibration to find spin-up and minimum stable duty
- Modern GUI (PyQt6 + pyqtgraph if available): Dashboard tiles (controls on top, curves/triggers below),
  Overview with hide-by-selection, sorting, rename, status-bar selection, theme & language toggle
- Headless CLI: --detect, --calibrate, --selftest
"""

from __future__ import annotations
import os, re, sys, time, json, argparse, tempfile, shutil, errno
from typing import Optional, Dict, List, Callable, Any, Tuple

# -----------------------
# Optional GUI stack
# -----------------------
GUI_AVAILABLE = False
QtWidgets = QtCore = QtGui = None
pg = None
try:
    from PyQt6 import QtWidgets as _QtWidgets, QtCore as _QtCore, QtGui as _QtGui
    QtWidgets, QtCore, QtGui = _QtWidgets, _QtCore, _QtGui
    GUI_AVAILABLE = True
    try:
        import pyqtgraph as _pg
        pg = _pg
    except Exception:
        pg = None
except Exception:
    GUI_AVAILABLE = False
    QtWidgets = QtCore = QtGui = None
    pg = None

APP_TITLE = "Linux Fan Control"
APP_VERSION = "0.4.0"  # bump when config format changes

# -----------------------
# I18N
# -----------------------
I18N = {
    "en": {
        "Dashboard": "Dashboard",
        "Overview": "Overview",
        "Controls": "Controls",
        "Curves": "Curves",
        "Curve": "Curve",
        "Edit": "Edit",
        "Add Channels": "Add Channels",
        "Detect & Calibrate": "Detect & Calibrate",
        "Cancel": "Cancel",
        "Create Channels": "Create Channels",
        "Auto-Setup started.": "Auto-Setup started.",
        "Sensor": "Sensor",
        "Rename": "Rename",
        "Hide selected": "Hide selected",
        "Sort by Type": "Sort by Type",
        "Sort by Name": "Sort by Name",
        "Language": "Language",
        "Theme": "Theme",
        "Light Theme": "Light Theme",
        "Dark Theme": "Dark Theme",
        "Profiles": "Profiles",
        "Add Profile": "Add Profile",
        "Delete Profile": "Delete Profile",
        "Save changes?": "Save changes?",
        "You have unsaved changes. Save before exit?": "You have unsaved changes. Save before exit?",
        "Save": "Save",
        "Discard": "Discard",
        "Status bar items": "Status bar items",
        "Controls (Channels)": "Controls (Channels)",
        "Curves/Triggers": "Curves/Triggers",
        "Mode": "Mode",
        "Auto": "Auto",
        "Manual": "Manual",
        "Hysteresis": "Hysteresis",
        "Response time": "Response time",
        "Manual duty": "Manual duty",
        "Label": "Label",
        "Close": "Close",
        "Saved": "Saved",
        "Select Channels": "Select Channels",
        "Use": "Use",
        "PWM Path": "PWM Path",
        "Enable Path": "Enable Path",
        "Tach Path": "Tach Path",
        "Sensor (temp)": "Sensor (temp)",
        "Sensor Type": "Sensor Type",
    },
    "de": {
        "Dashboard": "Dashboard",
        "Overview": "Übersicht",
        "Controls": "Steuerungen",
        "Curves": "Kurven",
        "Curve": "Kurve",
        "Edit": "Bearbeiten",
        "Add Channels": "Kanäle hinzufügen",
        "Detect & Calibrate": "Erkennen & Kalibrieren",
        "Cancel": "Abbrechen",
        "Create Channels": "Kanäle erstellen",
        "Auto-Setup started.": "Auto-Setup gestartet.",
        "Sensor": "Sensor",
        "Rename": "Umbenennen",
        "Hide selected": "Ausblenden",
        "Sort by Type": "Nach Typ sortieren",
        "Sort by Name": "Nach Name sortieren",
        "Language": "Sprache",
        "Theme": "Design",
        "Light Theme": "Helles Design",
        "Dark Theme": "Dunkles Design",
        "Profiles": "Profile",
        "Add Profile": "Profil hinzufügen",
        "Delete Profile": "Profil löschen",
        "Save changes?": "Änderungen speichern?",
        "You have unsaved changes. Save before exit?": "Nicht gespeicherte Änderungen. Vor dem Beenden speichern?",
        "Save": "Speichern",
        "Discard": "Verwerfen",
        "Status bar items": "Statusleisten-Anzeige",
        "Controls (Channels)": "Steuerungen (Kanäle)",
        "Curves/Triggers": "Kurven/Trigger",
        "Mode": "Modus",
        "Auto": "Automatisch",
        "Manual": "Manuell",
        "Hysteresis": "Hysterese",
        "Response time": "Antwortzeit",
        "Manual duty": "Manueller Wert",
        "Label": "Bezeichnung",
        "Close": "Schließen",
        "Saved": "Gespeichert",
        "Select Channels": "Kanäle auswählen",
        "Use": "Nutzen",
        "PWM Path": "PWM Pfad",
        "Enable Path": "Enable Pfad",
        "Tach Path": "Tacho Pfad",
        "Sensor (temp)": "Sensor (Temp.)",
        "Sensor Type": "Sensor-Typ",
    }
}
LANG = "de"
def t(key: str) -> str:
    return I18N.get(LANG, I18N["en"]).get(key, key)

# -----------------------
# Utilities
# -----------------------
def read_text(path: Optional[str]) -> Optional[str]:
    if not path:
        return None
    try:
        with open(path, "r") as f:
            return f.read().strip()
    except Exception:
        return None

def write_text(path: Optional[str], text: Optional[str]) -> bool:
    if not path or text is None:
        return False
    try:
        with open(path, "w") as f:
            f.write(text)
        return True
    except Exception:
        return False

def clamp(v: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, v))

def milli_to_c(val: float) -> float:
    # hwmon temps are often in millidegC
    if val > 200.0:
        return val / 1000.0
    return val

def now_monotonic() -> float:
    try:
        return time.monotonic()
    except Exception:
        return time.time()

# -----------------------
# Curves + Filter
# -----------------------
class Curve:
    """Piecewise linear curve with draggable control points."""
    def __init__(self, points: Optional[List[Tuple[float, float]]] = None):
        self.points: List[Tuple[float, float]] = sorted(points or [(20, 20), (40, 40), (60, 60), (80, 100)])

    def sorted_points(self) -> List[Tuple[float, float]]:
        self.points = sorted(self.points, key=lambda p: p[0])
        return list(self.points)

    def eval(self, x: float) -> float:
        pts = self.sorted_points()
        if not pts:
            return 0.0
        if x <= pts[0][0]:
            return pts[0][1]
        if x >= pts[-1][0]:
            return pts[-1][1]
        for i in range(1, len(pts)):
            x0, y0 = pts[i - 1]
            x1, y1 = pts[i]
            if x0 <= x <= x1:
                if x1 == x0:
                    return y1
                t_ = (x - x0) / (x1 - x0)
                return y0 + t_ * (y1 - y0)
        return pts[-1][1]

class SchmittWithSlew:
    """Schmitt-like hysteresis in input + first-order slew towards target."""
    def __init__(self, hysteresis_c: float = 0.0, tau_s: float = 0.0):
        self.hyst = max(0.0, hysteresis_c)
        self.tau = max(0.0, tau_s)
        self._last_y = 0.0
        self._last_x: Optional[float] = None
        self._dir_up = True
        self._t_last: Optional[float] = None

    def step(self, curve: Curve, x: float, now: Optional[float] = None) -> float:
        if self._last_x is not None:
            self._dir_up = x > self._last_x
        x_eff = x + (self.hyst / 2.0 if self._dir_up else -self.hyst / 2.0)
        target = clamp(curve.eval(x_eff), 0.0, 100.0)
        self._last_x = x

        if self.tau <= 0.0:
            self._last_y = target
            self._t_last = now or time.time()
            return self._last_y
        t_now = now or time.time()
        if self._t_last is None:
            self._t_last = t_now
            self._last_y = target
            return self._last_y
        dt = max(0.0, t_now - self._t_last)
        self._t_last = t_now
        if dt <= 0.0:
            return self._last_y
        # exponential approach approximation
        alpha = 1.0 - pow(2.718281828, -dt / self.tau)
        self._last_y = self._last_y + alpha * (target - self._last_y)
        return clamp(self._last_y, 0.0, 100.0)

# -----------------------
# Sensors & Outputs
# -----------------------
class SensorProvider:
    def __init__(self, name: str, unit: str = "°C"):
        self.name = name
        self.unit = unit
        self._last: Optional[float] = None

    def read(self) -> Optional[float]:
        raise NotImplementedError

    @property
    def last(self) -> Optional[float]:
        return self._last

class SysfsHwmonTemp(SensorProvider):
    """Simple polling sensor without Qt signals (MainWindow polls)."""
    def __init__(self, name: str, path: str, unit: str = "°C"):
        super().__init__(name, unit)
        self.path = path

    def read(self) -> Optional[float]:
        try:
            with open(self.path, "r") as f:
                raw = f.read().strip()
            v = float(raw)
            v = milli_to_c(v)
            self._last = v
            return v
        except Exception:
            return None

class OutputController:
    def __init__(self, name: str):
        self.name = name
        self.last_duty = 0.0

    def write(self, duty_percent: float):
        duty_percent = clamp(duty_percent, 0.0, 100.0)
        self.last_duty = duty_percent
        self._write_impl(duty_percent)

    def _write_impl(self, duty_percent: float):
        raise NotImplementedError

class SysfsPwmOutput(OutputController):
    def __init__(self, name: str, pwm_path: str, enable_path: Optional[str] = None,
                 min_pct: float = 0.0, max_pct: float = 100.0, writable: bool = True):
        super().__init__(name)
        self.pwm_path = pwm_path
        self.enable_path = enable_path
        self.min_pct = min_pct
        self.max_pct = max_pct
        self.writable = writable

    def _write_impl(self, duty_percent: float):
        if not self.writable:
            return
        duty_percent = clamp(duty_percent, self.min_pct, self.max_pct)
        pwm_val = int(round(duty_percent * 255.0 / 100.0))
        try:
            if self.enable_path and os.path.exists(self.enable_path):
                # try to force manual mode (for amdgpu etc.)
                try:
                    with open(self.enable_path, "w") as f:
                        f.write("1")
                except OSError as e:
                    if e.errno not in (errno.EROFS, errno.EPERM):
                        raise
            with open(self.pwm_path, "w") as f:
                f.write(str(pwm_val))
        except OSError as e:
            if e.errno == errno.EOPNOTSUPP or e.errno == 95:
                # Operation not supported -> mark read-only
                self.writable = False
                print(f"[WARN] PWM write({self.name}) not supported -> marked read-only")
            else:
                print(f"[WARN] PWM write({self.name}) failed: {e}")

# -----------------------
# Discovery & Classification
# -----------------------
def list_hwmon_tree(base: str = "/sys/class/hwmon") -> Dict[str, Dict[str, str]]:
    found: Dict[str, Dict[str, str]] = {}
    if not os.path.isdir(base):
        return found
    for d in sorted(os.listdir(base)):
        hw = os.path.join(base, d)
        if not os.path.isdir(hw):
            continue
        try:
            with open(os.path.join(hw, "name")) as f:
                devname = f.read().strip()
        except Exception:
            devname = d
        devmap: Dict[str, str] = {}
        for fpath in os.listdir(hw):
            full = os.path.join(hw, fpath)
            if (fpath.startswith("temp") and fpath.endswith("_input")) or (fpath.startswith("temp") and fpath.endswith("_label")):
                devmap[fpath] = full
            if fpath.startswith("fan") and fpath.endswith("_input"):
                devmap[fpath] = full
            if fpath.startswith("pwm") and (fpath[3:].isdigit() or fpath[3:].split('_')[0].isdigit()):
                devmap[fpath] = full
            if fpath.startswith("pwm") and fpath.endswith("_enable"):
                devmap[fpath] = full
        found[f"{d}:{devname}"] = devmap
    return found

SENSOR_NAME_HINTS = {
    'CPU':   [r'k10temp', r'coretemp', r'zenpower', r'pkgtemp'],
    'GPU':   [r'amdgpu', r'nvidia'],
    'NVMe':  [r'nvme'],
    'EC':    [r'asus[-_]?ec', r'ibm-ec', r'ec'],
    'SIO':   [r'(it8\d+|nct\d+|w83\d+)'],
}
SENSOR_LABEL_HINTS = {
    'CPU':   [r'\bcpu\b', r'tctl', r'package', r'tdie', r'core'],
    'GPU':   [r'\bgpu\b', r'junction', r'hotspot', r'edge', r'vram', r'hbm'],
    'Chipset': [r'chip|pch|smu|south|north'],
    'Motherboard': [r'mobo|mb|board|system|systin|case'],
    'VRM':   [r'vrm|mos|vcore'],
    'NVMe':  [r'nvme|ssd|composite'],
    'Water': [r'water|coolant|liquid'],
    'Ambient':[r'ambient|room'],
}
PCI_GPU_HINTS = ["/drm/card", "/pci", "/0000:"]
NVME_HINT = ["/nvme", "/block/nvme"]

_re_cache: Dict[str, re.Pattern] = {}
def _re(pat: str) -> re.Pattern:
    if pat not in _re_cache:
        _re_cache[pat] = re.compile(pat, re.IGNORECASE)
    return _re_cache[pat]

def _match_any(text: str, patterns: List[str]) -> bool:
    t = text or ""
    for pat in patterns:
        if _re(pat).search(t):
            return True
    return False

def _device_symlink(hwid: str) -> str:
    p = f"/sys/class/hwmon/{hwid}/device"
    try:
        return os.path.realpath(p)
    except Exception:
        return ""

def classify_sensor(hwmon_name: str, temp_label: Optional[str], hwid: Optional[str] = None) -> str:
    name = (hwmon_name or "").lower()
    lab  = (temp_label or "").lower()
    if hwid:
        devp = _device_symlink(hwid)
        try:
            if any(h in devp for h in PCI_GPU_HINTS) and ('amdgpu' in devp or 'drm' in devp or 'nvidia' in devp):
                return 'GPU'
            if any(h in devp for h in NVME_HINT):
                return 'NVMe'
        except Exception:
            pass
    for t, pats in SENSOR_NAME_HINTS.items():
        if _match_any(name, pats):
            if t in ('EC', 'SIO'):
                break
            return 'CPU' if t == 'CPU' else ('GPU' if t == 'GPU' else t)
    for t, pats in SENSOR_LABEL_HINTS.items():
        if _match_any(lab, pats):
            return t
    if _match_any(name, SENSOR_NAME_HINTS.get('EC', []) + SENSOR_NAME_HINTS.get('SIO', [])):
        return 'Motherboard'
    return 'Unknown'

def suggest_label(sensor_type: str, base_label: str) -> str:
    bl = (base_label or '').strip()
    if sensor_type != 'Unknown' and not bl.lower().startswith(sensor_type.lower()+':'):
        return f"{sensor_type}: {bl}"
    return bl or sensor_type

def discover_temp_sensors(base: str = "/sys/class/hwmon") -> List[Dict[str, str]]:
    tree = list_hwmon_tree(base)
    sensors: List[Dict[str, str]] = []
    for dev, entries in tree.items():
        hwid, hwname = (dev.split(':', 1) + [''])[:2]
        labels_map: Dict[str, str] = {}
        for k, p in entries.items():
            if k.startswith("temp") and k.endswith("_label"):
                try:
                    with open(p, 'r') as f:
                        labels_map[k.split('_')[0]] = f.read().strip()
                except Exception:
                    pass
        for k, p in entries.items():
            if k.startswith("temp") and k.endswith("_input"):
                tempn = k.split('_')[0]
                raw = labels_map.get(tempn, tempn)
                s_type = classify_sensor(hwname, raw, hwid)
                nice = suggest_label(s_type, raw)
                label = f"{dev}:{nice}"
                sensors.append({
                    "device": dev,
                    "label": label,
                    "raw_label": raw,
                    "type": s_type,
                    "path": p,
                    "unit": "°C",
                })
    return sensors

def discover_pwm_devices(base: str = "/sys/class/hwmon") -> List[Dict[str, Optional[str]]]:
    tree = list_hwmon_tree(base)
    devices = []
    for dev, entries in tree.items():
        pwms = {k: v for k, v in entries.items() if k.startswith("pwm") and k[3:].split('_')[0].isdigit() and not k.endswith("_enable")}
        for k, pwm_path in sorted(pwms.items()):
            n = ''.join(ch for ch in k if ch.isdigit())
            enable = entries.get(f"pwm{n}_enable")
            tach = entries.get(f"fan{n}_input") or None
            devices.append({
                "device": dev,
                "label": f"{dev}:{k}",
                "pwm_path": pwm_path,
                "enable_path": enable,
                "fan_input_path": tach,
            })
    return devices

def couple_outputs_to_sensors(sensors: List[Dict[str, str]], pwms: List[Dict[str, Optional[str]]]) -> Dict[str, Dict[str, str]]:
    mapping: Dict[str, Dict[str, str]] = {}
    def hwid(dev: str) -> str:
        return dev.split(':', 1)[0]
    by_hwid: Dict[str, List[Dict[str, str]]] = {}
    for s in sensors:
        by_hwid.setdefault(hwid(s['device']), []).append(s)
    def prefer(sensor_list: List[Dict[str, str]]) -> Optional[Dict[str, str]]:
        for key in ['Water', 'CPU', 'GPU', 'Ambient', 'NVMe', 'Chipset', 'Motherboard']:
            for s in sensor_list:
                if s.get('type') == key:
                    return s
        return sensor_list[0] if sensor_list else None
    for p in pwms:
        hw = hwid(p['device'])
        candidates = by_hwid.get(hw, [])
        chosen = prefer(candidates) if candidates else prefer(sensors)
        if chosen:
            mapping[p['label']] = {"sensor_label": chosen['label'], "sensor_path": chosen['path'], "sensor_type": chosen.get('type','Unknown')}
    return mapping

# -----------------------
# PWM writability probe
# -----------------------
def probe_pwm_writable(pwm_path: str, enable_path: Optional[str]) -> Tuple[bool, str]:
    """Try to make the PWM writable (enable manual), then attempt a no-op write & restore.
       Returns (writable, reason_if_false)."""
    try:
        prev_raw = read_text(pwm_path)
        prev_en = read_text(enable_path) if enable_path else None

        # Try to set manual mode "1" if present
        if enable_path and os.path.exists(enable_path) and prev_en != "1":
            try:
                with open(enable_path, "w") as f:
                    f.write("1")
            except OSError:
                # ignore, we'll still try the PWM write
                pass

        # Try writing same value back (no-op)
        if prev_raw is not None:
            try:
                with open(pwm_path, "w") as f:
                    f.write(prev_raw)
            except OSError as e:
                # restore enable
                if enable_path and prev_en is not None:
                    try: open(enable_path, "w").write(prev_en)
                    except Exception: pass
                if e.errno in (errno.EOPNOTSUPP, 95):
                    return (False, "operation not supported by kernel driver")
                if e.errno in (errno.EROFS, errno.EPERM):
                    return (False, "permission denied or read-only fs")
                return (False, f"write failed: {e}")
        # restore
        if enable_path and prev_en is not None:
            try: open(enable_path, "w").write(prev_en)
            except Exception: pass
        if prev_raw is not None:
            try: open(pwm_path, "w").write(prev_raw)
            except Exception: pass
        return (True, "")
    except Exception as e:
        return (False, f"probe error: {e}")

# -----------------------
# Calibration & Active Coupling Detection
# -----------------------
class Calibrator:
    def __init__(self, sleep: Callable[[float], None] = time.sleep):
        self.sleep = sleep

    @staticmethod
    def _read_rpm(fan_input_path: Optional[str]) -> Optional[int]:
        if not fan_input_path:
            return None
        try:
            with open(fan_input_path, 'r') as f:
                v = int(f.read().strip())
            return v
        except Exception:
            return None

    @staticmethod
    def _write_pwm(pwm_path: str, duty_percent: float):
        pwm_val = int(round(clamp(duty_percent, 0.0, 100.0) * 255 / 100))
        with open(pwm_path, 'w') as f:
            f.write(str(pwm_val))

    def calibrate(self, pwm_path: str, fan_input_path: Optional[str], enable_path: Optional[str] = None,
                  start: int = 0, end: int = 100, step: int = 5, settle_s: float = 1.0,
                  rpm_threshold: int = 100, floor_pct: int = 20,
                  cancelled: Optional[Callable[[], bool]] = None) -> Dict[str, Any]:
        """Sweep to find spin-up and min stable duty. Never goes below floor_pct."""
        # ensure manual if possible
        try:
            if enable_path and os.path.exists(enable_path):
                with open(enable_path, 'w') as f:
                    f.write('1')
        except Exception:
            pass

        def should_cancel() -> bool:
            return bool(cancelled and cancelled())

        def safe_write(pct: float):
            self._write_pwm(pwm_path, max(floor_pct, pct))

        spinup = None
        rpm_at_min = 0
        for duty in range(max(start, floor_pct), end + 1, step):
            if should_cancel():
                return {"ok": False, "aborted": True}
            try:
                safe_write(duty)
            except Exception as e:
                return {"ok": False, "error": f"write PWM failed: {e}"}
            self.sleep(settle_s)
            if should_cancel():
                return {"ok": False, "aborted": True}
            rpm = self._read_rpm(fan_input_path)
            if rpm is not None and rpm >= rpm_threshold:
                spinup = duty
                rpm_at_min = rpm
                break
        if spinup is None:
            return {"ok": False, "error": "No spin detected (tach missing or fan stopped)", "min_pct": end}

        min_stable = spinup
        for duty in range(spinup, max(start, floor_pct) - 1, -step):
            if should_cancel():
                return {"ok": False, "aborted": True}
            try:
                safe_write(duty)
            except Exception as e:
                return {"ok": False, "error": f"write PWM failed: {e}"}
            self.sleep(settle_s)
            if should_cancel():
                return {"ok": False, "aborted": True}
            rpm = self._read_rpm(fan_input_path)
            if rpm is not None and rpm >= rpm_threshold:
                min_stable = duty
            else:
                break

        result = {"ok": True, "spinup_pct": spinup, "min_pct": min_stable, "rpm_at_min": rpm_at_min}
        try:
            safe_write(min_stable)
        except Exception:
            pass
        return result

    def calibrate_all(self, pwms: List[Dict[str, Optional[str]]], start=0, end=100, step=5, settle_s=1.0, rpm_threshold=100,
                      floor_pct: int = 20,
                      progress: Optional[Callable[[int, int, str], None]] = None,
                      cancelled: Optional[Callable[[], bool]] = None) -> Dict[str, Any]:
        results: Dict[str, Any] = {}
        total = len(pwms)
        for i, d in enumerate(pwms):
            label = d['label']
            if progress:
                progress(i, total, label)
            res = self.calibrate(d['pwm_path'], d.get('fan_input_path'), d.get('enable_path'),
                                 start, end, step, settle_s, rpm_threshold, floor_pct, cancelled)
            results[label] = res
            if progress:
                progress(i + 1, total, label)
            if res.get('aborted'):
                break
        return {"ok": True, "results": results}

def infer_coupling_via_perturbation(
    pwms: List[Dict[str, Optional[str]]],
    temps: List[Dict[str, str]],
    *,
    hold_pct: int = 100,
    hold_s: float = 10.0,
    floor_pct: int = 20,
    progress: Optional[Callable[[str], None]] = None,
    cancelled: Optional[Callable[[], bool]] = None,
    # IO hooks for testing
    read_temp: Optional[Callable[[str], Optional[float]]] = None,
    read_pwm_raw: Optional[Callable[[str], Optional[str]]] = None,
    write_pwm_pct: Optional[Callable[[str, float], None]] = None,
    read_enable: Optional[Callable[[Optional[str]], Optional[str]]] = None,
    write_enable: Optional[Callable[[Optional[str], Optional[str]], None]] = None,
    sleep: Callable[[float], None] = time.sleep,
) -> Dict[str, Dict[str, str | float]]:
    """
    Return mapping {pwm_label: {sensor_label, sensor_path, score}} by boosting PWM to `hold_pct`
    for `hold_s` seconds (NEVER below floor). Restores pwm/enable afterward. Skips if PWM not writable.
    If tach is present and RPM does not change meaningfully, skip quickly.
    """
    def _log(msg: str):
        if progress:
            progress(msg)

    def _should_cancel() -> bool:
        return bool(cancelled and cancelled())

    def _read_temp_file(path: str) -> Optional[float]:
        if read_temp:
            return read_temp(path)
        try:
            with open(path, 'r') as f:
                s = f.read().strip()
            v = float(s)
            return milli_to_c(v)
        except Exception:
            return None

    def _read_pwm_raw_file(path: str) -> Optional[str]:
        if read_pwm_raw:
            return read_pwm_raw(path)
        try:
            with open(path, 'r') as f:
                return f.read().strip()
        except Exception:
            return None

    def _write_pwm_pct_file(path: str, pct: float):
        if write_pwm_pct:
            write_pwm_pct(path, pct)
            return
        val = int(round(clamp(pct, floor_pct, 100.0) * 255 / 100))
        with open(path, 'w') as f:
            f.write(str(val))

    def _read_enable_file(path: Optional[str]) -> Optional[str]:
        if read_enable:
            return read_enable(path)
        if not path:
            return None
        try:
            with open(path, 'r') as f:
                return f.read().strip()
        except Exception:
            return None

    def _write_enable_file(path: Optional[str], val: Optional[str]):
        if write_enable:
            write_enable(path, val)
            return
        if not path or val is None:
            return
        try:
            with open(path, 'w') as f:
                f.write(val)
        except Exception:
            pass

    def _pct_from_raw(raw: Optional[str]) -> Optional[float]:
        try:
            if raw is None:
                return None
            x = int(raw)
            if x < 0:
                return None
            return clamp(x * 100.0 / 255.0, 0.0, 100.0)
        except Exception:
            return None

    def _read_all_temps() -> Dict[str, float]:
        vals: Dict[str, float] = {}
        for t_ in temps:
            v = _read_temp_file(t_['path'])
            if v is not None:
                vals[t_['path']] = v
        return vals

    mapping: Dict[str, Dict[str, str | float]] = {}
    for d in pwms:
        if _should_cancel():
            _log("Coupling detection canceled")
            break
        label = d['label']
        pwm_path = d.get('pwm_path') or ''
        enable_path = d.get('enable_path')
        tach_path = d.get('fan_input_path')
        _log(f"Probing coupling for {label}…")

        # Probe writability (try to force manual mode for drivers like amdgpu)
        writable, reason = probe_pwm_writable(pwm_path, enable_path)
        if not writable:
            _log(f"  Skipping {label}: PWM not writable ({reason})")
            continue

        prev_en = _read_enable_file(enable_path)
        prev_pwm_raw = _read_pwm_raw_file(pwm_path)
        prev_pct = _pct_from_raw(prev_pwm_raw)
        start_pct = max(floor_pct, (prev_pct if prev_pct is not None else 35.0))
        # set manual if needed
        if prev_en is not None and prev_en != '1':
            _write_enable_file(enable_path, '1')

        # Optional quick tach probe: if tach exists and changing to hold_pct does not alter rpm, skip
        rpm_before = None
        try:
            if tach_path and os.path.exists(tach_path):
                with open(tach_path, 'r') as f:
                    rpm_before = int(f.read().strip())
        except Exception:
            rpm_before = None

        # Boost to hold_pct and hold for full 10 s (monotonic)
        try:
            _write_pwm_pct_file(pwm_path, max(floor_pct, hold_pct))
        except OSError as e:
            # restore and skip
            _log(f"  Skipping {label}: PWM write failed during boost: {e}")
            if prev_en is not None:
                _write_enable_file(enable_path, prev_en)
            if prev_pwm_raw is not None:
                try: open(pwm_path,'w').write(prev_pwm_raw)
                except Exception: pass
            continue

        t0 = now_monotonic()
        while now_monotonic() - t0 < hold_s:
            if _should_cancel():
                break
            sleep(0.1)

        rpm_after = None
        try:
            if tach_path and os.path.exists(tach_path):
                with open(tach_path, 'r') as f:
                    rpm_after = int(f.read().strip())
        except Exception:
            rpm_after = None

        if rpm_before is not None and rpm_after is not None:
            if abs(rpm_after - rpm_before) < 50:  # insignificant -> skip quickly
                _log(f"  Tach did not change significantly for {label} (ΔRPM={abs(rpm_after-rpm_before)}).")
                # restore snapshot
                if prev_pwm_raw is not None:
                    try: open(pwm_path, 'w').write(prev_pwm_raw)
                    except Exception: pass
                if prev_en is not None:
                    _write_enable_file(enable_path, prev_en)
                continue

        # Measure temperature deltas against baseline at start_pct
        _write_pwm_pct_file(pwm_path, start_pct)
        sleep(2.0)  # let return a bit
        t_base = _read_all_temps()

        # Boost again (measuring phase)
        _write_pwm_pct_file(pwm_path, max(floor_pct, hold_pct))
        t1 = now_monotonic()
        while now_monotonic() - t1 < hold_s:
            if _should_cancel():
                break
            sleep(0.1)
        t_high = _read_all_temps()

        # restore snapshot
        if prev_pwm_raw is not None:
            try: open(pwm_path, 'w').write(prev_pwm_raw)
            except Exception: pass
        if prev_en is not None:
            _write_enable_file(enable_path, prev_en)

        # Score by absolute temp delta (robust thresholding left to caller)
        best_path = None
        best_val = 0.0
        for t_ in temps:
            p = t_['path']
            if p in t_high and p in t_base:
                dtemp = abs(t_high[p] - t_base[p])
                if dtemp > best_val:
                    best_val = dtemp
                    best_path = p
        if best_path is not None:
            st = next((t_ for t_ in temps if t_['path'] == best_path), None)
            if st:
                mapping[label] = {"sensor_label": st['label'], "sensor_path": st['path'], "score": float(best_val)}
        _log(f"  → best sensor: {mapping.get(label, {}).get('sensor_label', 'n/a')} (score {best_val:.2f})")
    return mapping

# -----------------------
# Profile / Trigger / Channel
# -----------------------
class Trigger:
    """Reusable trigger (source sensor + curve/filter)."""
    def __init__(self, name: str, sensor_name: str, curve: Curve, hysteresis_c=0.0, tau_s=0.0):
        self.name = name
        self.sensor_name = sensor_name
        self.curve = curve
        self.hyst = hysteresis_c
        self.tau = tau_s

class TriggerManager:
    def __init__(self):
        self._triggers: Dict[str, Trigger] = {}
    def add(self, trig: Trigger):
        self._triggers[trig.name] = trig
    def get(self, name: str) -> Optional[Trigger]:
        return self._triggers.get(name)
    def names(self) -> List[str]:
        return list(self._triggers.keys())

class Channel:
    def __init__(self, name: str, sensor: SensorProvider, output: SysfsPwmOutput, curve: Curve,
                 hysteresis_c: float = 0.0, response_tau_s: float = 0.0, mode: str = "Auto",
                 trigger: Optional[Trigger] = None):
        self.name = name
        self.sensor = sensor
        self.output = output
        self.curve = curve
        self.schmitt = SchmittWithSlew(hysteresis_c, response_tau_s)
        self.mode = mode  # "Auto" or "Manual"
        self.manual_value = 0.0
        self.outputChanged: Optional[Callable[[float], None]] = None
        self.trigger = trigger

    def apply(self):
        if not self.output.writable:
            return
        if self.mode == "Manual":
            duty = self.manual_value
        else:
            sval = self.sensor.last if self.sensor.last is not None else self.sensor.read()
            if sval is None:
                return
            curve = self.trigger.curve if self.trigger else self.curve
            # use trigger params if present
            if self.trigger:
                self.schmitt.hyst = self.trigger.hyst
                self.schmitt.tau  = self.trigger.tau
            duty = self.schmitt.step(curve, float(sval))
        self.output.write(duty)
        if self.outputChanged:
            try:
                self.outputChanged(duty)
            except Exception:
                pass

class ProfileManager:
    def __init__(self):
        self._profiles: Dict[str, List[Channel]] = {}
        self._current: Optional[str] = None

    def add_profile(self, name: str, channels: List[Channel]):
        self._profiles[name] = channels
        if self._current is None:
            self._current = name

    def names(self) -> List[str]:
        return list(self._profiles.keys())

    def set_current(self, name: str):
        if name in self._profiles:
            self._current = name

    def get_channels(self) -> List[Channel]:
        if self._current and self._current in self._profiles:
            return self._profiles[self._current]
        return []

    def current_name(self) -> str:
        return self._current or ""

# -----------------------
# GUI widgets (Dashboard/Overview/Editors)
# -----------------------
if GUI_AVAILABLE:
    # ---- Mini curve view for cards ----
    class MiniCurveView(pg.PlotWidget if pg else QtWidgets.QWidget):  # type: ignore
        def __init__(self, curve: Curve, parent=None):
            if pg:
                super().__init__(parent=parent)
                self.setBackground(None)
                self.setFixedHeight(80)
                self.setMinimumWidth(160)
                self.showGrid(x=False, y=False)
                self.getPlotItem().hideAxis('left')
                self.getPlotItem().hideAxis('bottom')
                self.getPlotItem().setMouseEnabled(x=False, y=False)
                self.getPlotItem().setMenuEnabled(False)
                self._line = self.plot([], [], pen=pg.mkPen(width=2))
                self.set_curve(curve)
            else:
                super().__init__(parent)
                self._curve = curve
                self.setFixedHeight(80)
                self.setMinimumWidth(160)

        def set_curve(self, curve: Curve):
            if pg:
                pts = curve.sorted_points()
                self._line.setData([p[0] for p in pts], [p[1] for p in pts])
                self.getPlotItem().setRange(xRange=(min(p[0] for p in pts), max(p[0] for p in pts)),
                                            yRange=(0, 100), padding=0.05)
            else:
                self._curve = curve
                self.update()

        def paintEvent(self, ev):
            if pg:
                return super().paintEvent(ev)
            qp = QtGui.QPainter(self)
            qp.setRenderHint(QtGui.QPainter.RenderHint.Antialiasing, True)  # type: ignore
            rect = self.rect().adjusted(8, 8, -8, -8)
            qp.setPen(QtGui.QPen(QtGui.QColor("#6aa3ff"), 2))  # type: ignore
            pts = self._curve.sorted_points()
            if len(pts) >= 2:
                x0, x1 = pts[0][0], pts[-1][0]
                for i in range(1, len(pts)):
                    xa, ya = pts[i-1]; xb, yb = pts[i]
                    def mapx(x): return rect.left() + (x - x0) / (x1 - x0 + 1e-9) * rect.width()
                    def mapy(y): return rect.bottom() - (y / 100.0) * rect.height()
                    qp.drawLine(int(mapx(xa)), int(mapy(ya)), int(mapx(xb)), int(mapy(yb)))

    # ---- Channel card ----
    class ChannelCard(QtWidgets.QFrame):  # type: ignore
        editRequested = QtCore.pyqtSignal(object)  # type: ignore
        def __init__(self, channel: Channel, parent=None):
            super().__init__(parent)
            self.channel = channel
            self.setObjectName("card")
            self.setStyleSheet("QFrame#card{border-radius:10px;border:1px solid rgba(255,255,255,30);}")
            lay = QtWidgets.QVBoxLayout(self)
            lay.setContentsMargins(12, 10, 12, 10)
            lay.setSpacing(6)

            self.title = QtWidgets.QLabel(channel.name)
            self.title.setWordWrap(True)
            lay.addWidget(self.title)

            sub = QtWidgets.QHBoxLayout()
            src = channel.trigger.name if channel.trigger else "Curve"
            src_lbl = QtWidgets.QLabel(f"{src} · {channel.sensor.name.split(':',1)[-1]}")
            src_lbl.setToolTip(channel.sensor.name)
            sub.addWidget(src_lbl)
            sub.addStretch(1)
            self.out_lbl = QtWidgets.QLabel(f"{channel.output.last_duty:.0f}%")
            sub.addWidget(self.out_lbl)
            lay.addLayout(sub)

            self.mini = MiniCurveView(channel.trigger.curve if channel.trigger else channel.curve)
            lay.addWidget(self.mini)

            btn = QtWidgets.QPushButton(t("Edit"))
            btn.clicked.connect(lambda _=False: self.editRequested.emit(self.channel))
            lay.addWidget(btn)

        def set_output(self, duty: float):
            self.out_lbl.setText(f"{duty:.0f}%")
        def set_name(self, name: str):
            self.title.setText(name)

    # ---- Curve card ----
    class CurveCard(QtWidgets.QFrame):  # type: ignore
        editRequested = QtCore.pyqtSignal(object)  # type: ignore
        def __init__(self, title: str, curve: Curve, source_text: str, linked_channel: Optional[Channel], parent=None):
            super().__init__(parent)
            self.linked = linked_channel
            self.setObjectName("card")
            self.setStyleSheet("QFrame#card{border-radius:10px;border:1px solid rgba(255,255,255,30);}")
            lay = QtWidgets.QVBoxLayout(self)
            lay.setContentsMargins(12, 10, 12, 10)
            lay.setSpacing(6)

            self.title = QtWidgets.QLabel(title)
            self.title.setWordWrap(True)
            lay.addWidget(self.title)

            sub = QtWidgets.QHBoxLayout()
            src_lbl = QtWidgets.QLabel(f"{t('Sensor')}: {source_text}")
            sub.addWidget(src_lbl)
            sub.addStretch(1)
            self.val_lbl = QtWidgets.QLabel("-- %")
            sub.addWidget(self.val_lbl)
            lay.addLayout(sub)

            self.mini = MiniCurveView(curve)
            lay.addWidget(self.mini)

            btn = QtWidgets.QPushButton(t("Edit"))
            btn.clicked.connect(lambda _=False: self.editRequested.emit(self.linked))
            lay.addWidget(btn)

        def set_value_text(self, txt: str):
            self.val_lbl.setText(txt)

    # ---- Dashboard ----
    class DashboardWidget(QtWidgets.QScrollArea):  # type: ignore
        openChannelRequested = QtCore.pyqtSignal(object)  # type: ignore
        def __init__(self, parent=None):
            super().__init__(parent)
            self.setWidgetResizable(True)
            self._wrap = QtWidgets.QWidget()
            self.setWidget(self._wrap)

            self._vbox = QtWidgets.QVBoxLayout(self._wrap)
            self._vbox.setContentsMargins(10, 10, 10, 10)
            self._vbox.setSpacing(12)

            self.gb_controls = QtWidgets.QGroupBox(t("Controls"))
            self._grid_controls = QtWidgets.QGridLayout(self.gb_controls)
            self._grid_controls.setContentsMargins(10, 10, 10, 10)
            self._grid_controls.setHorizontalSpacing(10)
            self._grid_controls.setVerticalSpacing(10)
            self._vbox.addWidget(self.gb_controls)

            self.gb_curves = QtWidgets.QGroupBox(t("Curves"))
            self._grid_curves = QtWidgets.QGridLayout(self.gb_curves)
            self._grid_curves.setContentsMargins(10, 10, 10, 10)
            self._grid_curves.setHorizontalSpacing(10)
            self._grid_curves.setVerticalSpacing(10)
            self._vbox.addWidget(self.gb_curves)

            self._card_by_channel: Dict[Channel, ChannelCard] = {}

        def retranslate(self):
            self.gb_controls.setTitle(t("Controls"))
            self.gb_curves.setTitle(t("Curves"))

        def rebuild(self, channels: List[Channel], triggers: Optional["TriggerManager"]=None):
            for grid in (self._grid_controls, self._grid_curves):
                for i in reversed(range(grid.count())):
                    w = grid.itemAt(i).widget()
                    if w:
                        w.setParent(None)
            self._card_by_channel.clear()

            cols = 4
            for idx, ch in enumerate(channels):
                card = ChannelCard(ch)
                card.editRequested.connect(lambda c=ch: self.openChannelRequested.emit(c))
                r, c = divmod(idx, cols)
                self._grid_controls.addWidget(card, r, c)
                self._card_by_channel[ch] = card

            cur_idx = 0
            if triggers:
                for name in sorted(triggers.names()):
                    trg = triggers.get(name)
                    if not trg: continue
                    linked = next((ch for ch in channels if ch.trigger and ch.trigger.name == name), None)
                    src_txt = trg.sensor_name.split(':',1)[-1] if trg.sensor_name else ""
                    cc = CurveCard(name, trg.curve, src_txt, linked)
                    cc.editRequested.connect(lambda ch=linked: (self.openChannelRequested.emit(ch) if ch else None))
                    r, c = divmod(cur_idx, cols); cur_idx += 1
                    self._grid_curves.addWidget(cc, r, c)
            for ch in channels:
                if ch.trigger:
                    continue
                title = f"{t('Curve')} · {ch.name}"
                src_txt = ch.sensor.name.split(':',1)[-1]
                cc = CurveCard(title, ch.curve, src_txt, ch)
                cc.editRequested.connect(lambda ch=ch: self.openChannelRequested.emit(ch))
                r, c = divmod(cur_idx, cols); cur_idx += 1
                self._grid_curves.addWidget(cc, r, c)

        def update_output(self, ch: Channel, duty: float):
            card = self._card_by_channel.get(ch)
            if card:
                card.set_output(duty)

    # ---- Curve editor ----
    class CurveEditor(pg.PlotWidget if pg else QtWidgets.QWidget):  # type: ignore
        changed = QtCore.pyqtSignal()  # type: ignore
        def __init__(self, curve: Curve, x_label="Temp (°C)", y_label="Duty (%)", x_range=(0, 100), y_range=(0, 100), parent=None):
            if pg:
                super().__init__(parent=parent)
                self.setBackground('w')
                self.showGrid(x=True, y=True, alpha=0.3)
                self.getPlotItem().setLabel('bottom', x_label)
                self.getPlotItem().setLabel('left', y_label)
                self.getPlotItem().setRange(xRange=x_range, yRange=y_range)
                self.xmin, self.xmax = x_range
                self.ymin, self.ymax = y_range
                self.curve = curve
                self.line = self.plot([], [], pen=pg.mkPen(width=2))
                self.scatter = pg.ScatterPlotItem(size=12, brush=pg.mkBrush(100, 100, 255, 180))
                self.addItem(self.scatter)
                self._dragging_index: Optional[int] = None
                self._update_graph()
                self.scatter.sigClicked.connect(self._on_point_clicked)
                self.scene().sigMouseClicked.connect(self._on_mouse_click)
            else:
                super().__init__(parent)
                l = QtWidgets.QVBoxLayout(self)
                l.addWidget(QtWidgets.QLabel("pyqtgraph not available."))

        def _update_graph(self):
            if not pg:
                return
            pts = self.curve.sorted_points()
            xs = [p[0] for p in pts]
            ys = [p[1] for p in pts]
            self.line.setData(xs, ys)
            spots = [{'pos': (x, y), 'data': i} for i, (x, y) in enumerate(pts)]
            self.scatter.setData(spots)

        def _mouse_to_plot(self, ev):
            if not pg:
                return None
            vb = self.getPlotItem().getViewBox()
            if vb is None:
                return None
            p = ev.scenePos()
            if not vb.sceneBoundingRect().contains(p):
                return None
            mp = vb.mapSceneToView(p)
            x = min(self.xmax, max(self.xmin, mp.x()))
            y = min(self.ymax, max(self.ymin, mp.y()))
            return x, y

        def _on_point_clicked(self, item, points):
            if points:
                self._dragging_index = points[0].data()

        def _on_mouse_click(self, ev):
            if not pg:
                return
            dbl = getattr(ev, "double", lambda: False)()
            if ev.button() == QtCore.Qt.MouseButton.LeftButton:
                if dbl:
                    mp = self._mouse_to_plot(ev)
                    if mp is None:
                        return
                    x, y = mp
                    self.curve.points.append((x, y))
                    self.curve.points = self.curve.sorted_points()
                    self._update_graph()
                    self.changed.emit()
                else:
                    self._dragging_index = None
            elif ev.button() == QtCore.Qt.MouseButton.RightButton:
                mp = self._mouse_to_plot(ev)
                if mp is None or len(self.curve.points) <= 2:
                    return
                x, y = mp
                idx = min(range(len(self.curve.points)), key=lambda i: (self.curve.points[i][0]-x)**2 + (self.curve.points[i][1]-y)**2)
                self.curve.points.pop(idx)
                self._update_graph()
                self.changed.emit()

        def mouseMoveEvent(self, ev):
            if not pg:
                return super().mouseMoveEvent(ev)
            if getattr(self, "_dragging_index", None) is None:
                return super().mouseMoveEvent(ev)
            vb = self.getPlotItem().getViewBox()
            if vb is None:
                return super().mouseMoveEvent(ev)
            mp = vb.mapSceneToView(ev.position())
            x = min(self.xmax, max(self.xmin, mp.x()))
            y = min(self.ymax, max(self.ymin, mp.y()))
            pts = self.curve.sorted_points()
            idx = self._dragging_index
            x_left = pts[idx-1][0] + 0.1 if idx > 0 else self.xmin
            x_right = pts[idx+1][0] - 0.1 if idx < len(pts)-1 else self.xmax
            x = min(x_right, max(x_left, x))
            pts[idx] = (x, y)
            self.curve.points = pts
            self._update_graph()
            self.changed.emit()

    # ---- Channel editor dialog ----
    class ChannelDialog(QtWidgets.QDialog):  # type: ignore
        def __init__(self, channel: Channel, parent=None):
            super().__init__(parent)
            self.setWindowTitle(t("Edit"))
            self.setModal(True)
            self.channel = channel
            v = QtWidgets.QVBoxLayout(self)

            form = QtWidgets.QFormLayout()
            self.name_edit = QtWidgets.QLineEdit(channel.name)
            form.addRow(t("Label")+":", self.name_edit)

            self.mode_combo = QtWidgets.QComboBox(); self.mode_combo.addItems([t("Auto"), t("Manual")])
            self.mode_combo.setCurrentText(t(channel.mode))
            form.addRow(t("Mode")+":", self.mode_combo)

            self.hyst = QtWidgets.QDoubleSpinBox(); self.hyst.setRange(0.0, 20.0); self.hyst.setSuffix(" °C"); self.hyst.setValue(channel.schmitt.hyst)
            form.addRow(t("Hysteresis")+":", self.hyst)

            self.tau  = QtWidgets.QDoubleSpinBox(); self.tau.setRange(0.0, 60.0); self.tau.setSuffix(" s");  self.tau.setValue(channel.schmitt.tau)
            form.addRow(t("Response time")+":", self.tau)

            self.manual_slider = QtWidgets.QSlider(QtCore.Qt.Orientation.Horizontal); self.manual_slider.setRange(0, 100); self.manual_slider.setValue(int(channel.manual_value))
            form.addRow(t("Manual duty")+":", self.manual_slider)

            v.addLayout(form)

            self.editor = CurveEditor(channel.trigger.curve if channel.trigger else channel.curve)
            v.addWidget(self.editor)

            hb = QtWidgets.QHBoxLayout()
            self.lbl_saved = QtWidgets.QLabel("")
            hb.addWidget(self.lbl_saved)
            hb.addStretch(1)
            btn_close = QtWidgets.QPushButton(t("Close"))
            hb.addWidget(btn_close)
            v.addLayout(hb)

            def on_change():
                # save immediately into channel
                channel.name = self.name_edit.text()
                m = self.mode_combo.currentText()
                channel.mode = "Auto" if m == t("Auto") else "Manual"
                channel.schmitt.hyst = float(self.hyst.value())
                channel.schmitt.tau  = float(self.tau.value())
                channel.manual_value = float(self.manual_slider.value())
                if channel.trigger:
                    channel.trigger.curve = self.editor.curve
                else:
                    channel.curve = self.editor.curve
                self.lbl_saved.setText(t("Saved"))

            self.name_edit.textChanged.connect(lambda _: on_change())
            self.mode_combo.currentTextChanged.connect(lambda _: on_change())
            self.hyst.valueChanged.connect(lambda _: on_change())
            self.tau.valueChanged.connect(lambda _: on_change())
            self.manual_slider.valueChanged.connect(lambda _: on_change())
            self.editor.changed.connect(on_change)
            btn_close.clicked.connect(self.accept)

    # ---- Selection dialog after auto-setup ----
    class DetectCalibrateDialog(QtWidgets.QDialog):  # type: ignore
        def __init__(self, parent=None):
            super().__init__(parent)
            self.setWindowTitle(t("Select Channels"))
            self.resize(900, 560)
            v = QtWidgets.QVBoxLayout(self)
            self.table = QtWidgets.QTableWidget(0, 7)
            self.table.setHorizontalHeaderLabels([t("Use"), t("Label"), "PWM", t("Enable Path"), t("Tach Path"), t("Sensor (temp)"), t("Sensor Type")])
            self.table.horizontalHeader().setStretchLastSection(True)
            self.table.verticalHeader().setVisible(False)
            v.addWidget(self.table)
            h = QtWidgets.QHBoxLayout()
            self.btn_add = QtWidgets.QPushButton(t("Create Channels"))
            h.addStretch(1); h.addWidget(self.btn_add)
            v.addLayout(h)
            self.btn_add.clicked.connect(self.accept)

        def populate_from(self, pwms, mapping):
            self.table.setRowCount(0)
            for d in pwms:
                r = self.table.rowCount(); self.table.insertRow(r)
                chk = QtWidgets.QCheckBox(); chk.setChecked(True); self.table.setCellWidget(r, 0, chk)
                self.table.setItem(r, 1, QtWidgets.QTableWidgetItem(d["label"]))
                self.table.setItem(r, 2, QtWidgets.QTableWidgetItem(d.get("pwm_path") or ""))
                self.table.setItem(r, 3, QtWidgets.QTableWidgetItem(d.get("enable_path") or ""))
                self.table.setItem(r, 4, QtWidgets.QTableWidgetItem(d.get("fan_input_path") or ""))
                rec = mapping.get(d['label'], {})
                sensor_label = rec.get('sensor_label', '')
                self.table.setItem(r, 5, QtWidgets.QTableWidgetItem(sensor_label))
                self.table.setItem(r, 6, QtWidgets.QTableWidgetItem(rec.get('sensor_type','')))

        def selected(self) -> List[dict]:
            out = []
            for r in range(self.table.rowCount()):
                if self.table.cellWidget(r, 0).isChecked():
                    out.append({
                        "label": self.table.item(r, 1).text(),
                        "pwm": self.table.item(r, 2).text(),
                        "enable": self.table.item(r, 3).text(),
                        "tach": self.table.item(r, 4).text(),
                        "sensor_label": self.table.item(r, 5).text(),
                    })
            return out

    # ---- Worker for auto setup ----
    class AutoSetupWorker(QtCore.QObject):  # type: ignore
        started = QtCore.pyqtSignal()  # type: ignore
        progress = QtCore.pyqtSignal(int, int, str)  # type: ignore
        log = QtCore.pyqtSignal(str)
        finished = QtCore.pyqtSignal(dict)
        canceled = QtCore.pyqtSignal()
        def __init__(self):
            super().__init__()
            self._cancel = False

        @QtCore.pyqtSlot()
        def run(self):
            self.started.emit()
            self.log.emit("Detecting sensors & PWM outputs…")
            temps = discover_temp_sensors()
            pwms = discover_pwm_devices()
            self.log.emit(f"Found {len(temps)} sensors, {len(pwms)} PWM outputs.")

            # Snapshot current pwm/enable
            snapshots = {}
            for d in pwms:
                en = read_text(d.get('enable_path'))
                pwm_raw = read_text(d.get('pwm_path'))
                snapshots[d['label']] = {"enable": en, "pwm": pwm_raw}

            # Active coupling detection with 100% / 10s boost
            self.log.emit("Inferring coupling by 100% boost (safe floor, no 0%)…")
            mapping_active = infer_coupling_via_perturbation(
                pwms, temps,
                hold_pct=100, hold_s=10.0, floor_pct=20,
                progress=lambda s: self.log.emit(s),
                cancelled=lambda: self._cancel,
            )
            # Heuristic fallback
            mapping_heur = couple_outputs_to_sensors(temps, pwms)
            mapping: Dict[str, Any] = dict(mapping_active)
            for k, v in mapping_heur.items():
                mapping.setdefault(k, v)

            if self._cancel:
                self.log.emit("Cancel requested — restoring previous PWM states…")
                self._restore_all(pwms, snapshots)
                self.canceled.emit(); return

            # Calibration (independent of detection)
            cal = Calibrator()
            results: Dict[str, Any] = {}
            total = len(pwms)
            for i, d in enumerate(pwms):
                if self._cancel:
                    self.log.emit("Cancel requested — restoring previous PWM states…")
                    self._restore_all(pwms, snapshots)
                    self.canceled.emit(); return
                lbl = d['label']
                self.progress.emit(i, total, lbl)
                self.log.emit(f"Calibrating {lbl}…")
                res = cal.calibrate(d['pwm_path'], d.get('fan_input_path'), d.get('enable_path'),
                                    floor_pct=20, cancelled=lambda: self._cancel)
                results[lbl] = res
                # Restore snapshot after each device
                snap = snapshots.get(lbl, {})
                write_text(d.get('enable_path'), snap.get('enable'))
                write_text(d.get('pwm_path'), snap.get('pwm'))
                self.progress.emit(i + 1, total, lbl)
                if res.get('ok'):
                    self.log.emit(f"  → min_pct={res.get('min_pct')} (spinup={res.get('spinup_pct')})")
                elif res.get('aborted'):
                    self.log.emit("  → aborted by user")
                    self.log.emit("Restoring previous PWM states…")
                    self._restore_all(pwms, snapshots)
                    self.canceled.emit(); return
                else:
                    self.log.emit(f"  → failed: {res.get('error')}")
            # final restore for safety
            self._restore_all(pwms, snapshots)
            self.finished.emit({'sensors': temps, 'pwms': pwms, 'cal_res': results, 'mapping': mapping})

        def _restore_all(self, pwms, snapshots):
            for d in pwms:
                snap = snapshots.get(d['label'], {})
                write_text(d.get('enable_path'), snap.get('enable'))
                write_text(d.get('pwm_path'), snap.get('pwm'))

        def request_cancel(self):
            self._cancel = True

    # ---- Auto-setup dialog with progress + log ----
    class AutoSetupDialog(QtWidgets.QDialog):  # type: ignore
        def __init__(self, parent=None):
            super().__init__(parent)
            self.setWindowTitle(t("Detect & Calibrate"))
            self.resize(760, 440)
            v = QtWidgets.QVBoxLayout(self)
            self.label = QtWidgets.QLabel("Starting…")
            self.bar = QtWidgets.QProgressBar(); self.bar.setRange(0, 0)
            self.log = QtWidgets.QPlainTextEdit(); self.log.setReadOnly(True)
            h = QtWidgets.QHBoxLayout()
            self.btn_cancel = QtWidgets.QPushButton(t("Cancel"))
            h.addStretch(1); h.addWidget(self.btn_cancel)
            v.addWidget(self.label); v.addWidget(self.bar); v.addWidget(self.log); v.addLayout(h)
            self._thread = QtCore.QThread(self)
            self._worker = AutoSetupWorker()
            self._worker.moveToThread(self._thread)
            self._thread.started.connect(self._worker.run)
            self._worker.started.connect(lambda: self._append(t("Auto-Setup started.")))
            self._worker.progress.connect(self._on_progress)
            self._worker.log.connect(self._append)
            self._worker.finished.connect(self._on_finished)
            self._worker.canceled.connect(self._on_canceled)
            self.btn_cancel.clicked.connect(self._on_cancel_clicked)
            self._result: dict | None = None

        def _append(self, msg: str):
            self.log.appendPlainText(msg)

        def _on_progress(self, cur: int, total: int, label: str):
            if total <= 0:
                self.bar.setRange(0, 0)
            else:
                if self.bar.maximum() != total:
                    self.bar.setRange(0, total)
                self.bar.setValue(cur)
            self.label.setText(f"{cur}/{total} – {label}")

        def _on_cancel_clicked(self):
            self.btn_cancel.setEnabled(False)
            self.btn_cancel.setText(t("Cancel")+"…")
            self._worker.request_cancel()

        def _on_finished(self, result: Dict[str, Any]):
            self._result = result
            self._thread.quit(); self._thread.wait(10000)
            self.accept()

        def _on_canceled(self):
            self._result = None
            self._thread.quit(); self._thread.wait(10000)
            self.reject()

        def exec_and_wait(self) -> dict | None:
            self._thread.start()
            code = self.exec()
            return self._result if code == QtWidgets.QDialog.DialogCode.Accepted else None

    # ---- Overview (sensor list + hide by selection, sorting, rename) ----
    class OverviewWidget(QtWidgets.QWidget):  # type: ignore
        openChannelRequested = QtCore.pyqtSignal(object)  # type: ignore
        def __init__(self, parent=None):
            super().__init__(parent)
            v = QtWidgets.QVBoxLayout(self)

            # toolbar row
            hb = QtWidgets.QHBoxLayout()
            self.btn_hide = QtWidgets.QPushButton(t("Hide selected"))
            self.btn_sort_type = QtWidgets.QPushButton(t("Sort by Type"))
            self.btn_sort_name = QtWidgets.QPushButton(t("Sort by Name"))
            hb.addWidget(self.btn_hide); hb.addWidget(self.btn_sort_type); hb.addWidget(self.btn_sort_name); hb.addStretch(1)
            v.addLayout(hb)

            self.sensors_table = QtWidgets.QTableWidget(0, 4)
            self.sensors_table.setHorizontalHeaderLabels([t("Use"), "Type", "Title", "Value"])
            self.sensors_table.horizontalHeader().setStretchLastSection(True)
            self.sensors_table.verticalHeader().setVisible(False)
            self.sensors_table.setSelectionBehavior(QtWidgets.QAbstractItemView.SelectionBehavior.SelectRows)
            v.addWidget(self.sensors_table)

            # channels table
            self.channels_table = QtWidgets.QTableWidget(0, 5)
            self.channels_table.setHorizontalHeaderLabels(["Name", t("Mode"), "Sensor", "Output %", t("Edit")])
            self.channels_table.horizontalHeader().setStretchLastSection(False)
            self.channels_table.horizontalHeader().setSectionResizeMode(0, QtWidgets.QHeaderView.ResizeMode.Stretch)
            self.channels_table.horizontalHeader().setSectionResizeMode(2, QtWidgets.QHeaderView.ResizeMode.Stretch)
            self.channels_table.verticalHeader().setVisible(False)
            v.addWidget(self.channels_table)

            self._sensor_rows: Dict[str, int] = {}
            self._channel_rows: Dict[Channel, int] = {}
            self._hidden: set[str] = set()
            self._type_lookup: Dict[str, str] = {}

            self.btn_hide.clicked.connect(self._apply_hide)
            self.btn_sort_type.clicked.connect(lambda: self.sensors_table.sortItems(1))
            self.btn_sort_name.clicked.connect(lambda: self.sensors_table.sortItems(2))

            # context menu for rename
            self.sensors_table.setContextMenuPolicy(QtCore.Qt.ContextMenuPolicy.CustomContextMenu)
            self.sensors_table.customContextMenuRequested.connect(self._ctx_menu)

        def set_hidden(self, hidden: set[str]):
            self._hidden = set(hidden)

        def rebuild_sensors(self, sensors: List[SensorProvider], type_lookup: Dict[str, str]):
            self._type_lookup = dict(type_lookup)
            self.sensors_table.setRowCount(0)
            self._sensor_rows.clear()
            for s in sensors:
                r = self.sensors_table.rowCount(); self.sensors_table.insertRow(r)
                chk = QtWidgets.QCheckBox(); chk.setChecked(s.name not in self._hidden)
                # make entire first cell clickable
                w = QtWidgets.QWidget(); layout = QtWidgets.QHBoxLayout(w); layout.setContentsMargins(10,0,0,0); layout.addWidget(chk); layout.addStretch(1)
                self.sensors_table.setCellWidget(r, 0, w)
                typ = type_lookup.get(s.name, 'Unknown')
                self.sensors_table.setItem(r, 1, QtWidgets.QTableWidgetItem(typ))
                self.sensors_table.setItem(r, 2, QtWidgets.QTableWidgetItem(s.name))
                self.sensors_table.setItem(r, 3, QtWidgets.QTableWidgetItem("--"))
                chk.stateChanged.connect(lambda _v, nm=s.name: self._toggle_pending(nm))
                self._sensor_rows[s.name] = r
            self.sensors_table.resizeColumnsToContents()

        def _toggle_pending(self, name: str):
            # only mark; actual hide with button
            pass

        def _apply_hide(self):
            to_hide: List[str] = []
            for r in range(self.sensors_table.rowCount()):
                cellw = self.sensors_table.cellWidget(r,0)
                chk = cellw.findChild(QtWidgets.QCheckBox) if cellw else None
                nm = self.sensors_table.item(r,2).text()
                if chk and not chk.isChecked():
                    to_hide.append(nm)
            self._hidden = set(to_hide)

        def hidden(self) -> set[str]:
            return set(self._hidden)

        def _ctx_menu(self, pos):
            r = self.sensors_table.currentRow()
            if r < 0: return
            nm = self.sensors_table.item(r,2).text()
            menu = QtWidgets.QMenu(self)
            act_rename = menu.addAction(t("Rename"))
            a = menu.exec(self.sensors_table.viewport().mapToGlobal(pos))
            if a == act_rename:
                new, ok = QtWidgets.QInputDialog.getText(self, t("Rename"), t("Label")+":", text=nm)
                if ok and new:
                    self.sensors_table.item(r,2).setText(new)

        def update_sensor_value(self, name: str, text: str):
            if name in self._hidden:
                return
            r = self._sensor_rows.get(name)
            if r is not None and self.sensors_table.item(r,3):
                self.sensors_table.item(r, 3).setText(text)

        def rebuild_channels(self, channels: List[Channel]):
            self.channels_table.setRowCount(0)
            self._channel_rows.clear()
            for ch in channels:
                r = self.channels_table.rowCount(); self.channels_table.insertRow(r)
                self.channels_table.setItem(r, 0, QtWidgets.QTableWidgetItem(ch.name))
                self.channels_table.setItem(r, 1, QtWidgets.QTableWidgetItem(t(ch.mode)))
                self.channels_table.setItem(r, 2, QtWidgets.QTableWidgetItem(ch.sensor.name))
                self.channels_table.setItem(r, 3, QtWidgets.QTableWidgetItem(f"{ch.output.last_duty:.0f}"))
                btn = QtWidgets.QPushButton(t("Edit"))
                btn.clicked.connect(lambda _=False, c=ch: self.openChannelRequested.emit(c))
                self.channels_table.setCellWidget(r, 4, btn)
                self._channel_rows[ch] = r

        def update_channel_output(self, ch: Channel, duty: float):
            r = self._channel_rows.get(ch)
            if r is not None and self.channels_table.item(r,3):
                self.channels_table.item(r, 3).setText(f"{duty:.0f}")

    # ---- Main window ----
    class MainWindow(QtWidgets.QMainWindow):  # type: ignore
        def __init__(self, config: dict | None):
            super().__init__()
            self.setWindowTitle(APP_TITLE)
            self.resize(1280, 860)
            self._config_path = os.path.join(os.path.expanduser("~/.config/linux_fan_control"), "config.json")
            os.makedirs(os.path.dirname(self._config_path), exist_ok=True)

            # State
            self.sensors: Dict[str, SysfsHwmonTemp] = {}
            self.outputs: Dict[str, SysfsPwmOutput] = {}
            self.profile_mgr = ProfileManager()
            self.sensor_types: Dict[str, str] = {}
            self.triggers = TriggerManager()
            self._hidden_sensors: set[str] = set()
            self._statusbar_show: set[str] = set()
            self._dirty = False
            self._dark = True

            # Toolbar
            tb = self.addToolBar("Tools")
            tb.setMovable(False)
            self.profile_combo = QtWidgets.QComboBox(); self.profile_combo.currentTextChanged.connect(self._on_profile_switch)
            tb.addWidget(QtWidgets.QLabel(t("Profiles")+":")); tb.addWidget(self.profile_combo)
            btn_add_prof = QtWidgets.QPushButton("+"); btn_del_prof = QtWidgets.QPushButton("–")
            tb.addWidget(btn_add_prof); tb.addWidget(btn_del_prof)
            btn_add_prof.clicked.connect(self._add_profile)
            btn_del_prof.clicked.connect(self._del_profile)
            tb.addSeparator()
            self.act_detect = QtGui.QAction(t("Detect & Calibrate"), self)  # type: ignore
            self.act_detect.triggered.connect(self._auto_setup)  # type: ignore
            tb.addAction(self.act_detect)
            tb.addSeparator()
            self.btn_theme = QtWidgets.QPushButton(t("Light Theme") if self._dark else t("Dark Theme"))
            self.btn_lang = QtWidgets.QPushButton("EN/DE")
            tb.addWidget(self.btn_theme); tb.addWidget(self.btn_lang)
            self.btn_theme.clicked.connect(self._toggle_theme)
            self.btn_lang.clicked.connect(self._toggle_lang)

            # Central tabs
            self.tabs = QtWidgets.QTabWidget(); self.setCentralWidget(self.tabs)
            self.dashboard = DashboardWidget(self); self.tabs.addTab(self.dashboard, t("Dashboard"))
            self.overview = OverviewWidget(self); self.overview.openChannelRequested.connect(self._open_channel_tab)
            self.tabs.addTab(self.overview, t("Overview"))

            # Status bar with user-selectable items
            self.status_bar_labels: Dict[str, QtWidgets.QLabel] = {}
            self._status_menu = QtWidgets.QMenu(t("Status bar items"), self)
            self.menuBar().addMenu(self._status_menu)
            self._status_menu.aboutToShow.connect(self._rebuild_status_menu)

            # Main tick (sensor poll & channel apply)
            self.loop = QtCore.QTimer(); self.loop.setInterval(1000); self.loop.timeout.connect(self._tick); self.loop.start()

            # Load config or run auto-setup
            if config is None:
                cfg = self._load_config()
            else:
                cfg = config
            if not cfg:
                self._auto_setup()
            else:
                self._build_from_config(cfg)

        # ----- Config -----
        def _load_config(self) -> Optional[dict]:
            try:
                if not os.path.exists(self._config_path):
                    return None
                with open(self._config_path, "r") as f:
                    cfg = json.load(f)
                if cfg.get("version") != APP_VERSION:
                    # keep simple: accept older for now; in real app convert with backup
                    pass
                global LANG
                LANG = cfg.get("lang", "de")
                self._dark = cfg.get("dark", True)
                self._hidden_sensors = set(cfg.get("hidden", []))
                self._statusbar_show = set(cfg.get("statusbar", []))
                self._apply_theme()
                return cfg
            except Exception:
                return None

        def _save_config(self):
            try:
                data = {
                    "version": APP_VERSION,
                    "lang": LANG,
                    "dark": self._dark,
                    "hidden": list(self._hidden_sensors),
                    "statusbar": list(self._statusbar_show),
                    "profiles": {},
                }
                for name, channels in self.profile_mgr._profiles.items():
                    arr = []
                    for ch in channels:
                        arr.append({
                            "name": ch.name,
                            "sensor": ch.sensor.name,
                            "output": {"label": ch.output.name, "pwm": ch.output.pwm_path, "enable": ch.output.enable_path, "min_pct": ch.output.min_pct, "max_pct": ch.output.max_pct, "writable": ch.output.writable},
                            "curve": ch.curve.points,
                            "mode": ch.mode,
                            "hyst": ch.schmitt.hyst,
                            "tau": ch.schmitt.tau,
                            "manual": ch.manual_value,
                            "trigger": (ch.trigger.name if ch.trigger else None),
                        })
                    data["profiles"][name] = arr
                with open(self._config_path, "w") as f:
                    json.dump(data, f, indent=2)
                self._dirty = False
            except Exception as e:
                print("[WARN] save failed:", e)

        # ----- Theme / Language -----
        def _apply_theme(self):
            app = QtWidgets.QApplication.instance()
            if not app:
                return
            if self._dark:
                app.setStyle("Fusion")
                pal = QtGui.QPalette()
                pal.setColor(QtGui.QPalette.ColorRole.Window, QtGui.QColor(53,53,53))
                pal.setColor(QtGui.QPalette.ColorRole.WindowText, QtCore.Qt.GlobalColor.white)
                pal.setColor(QtGui.QPalette.ColorRole.Base, QtGui.QColor(35,35,35))
                pal.setColor(QtGui.QPalette.ColorRole.AlternateBase, QtGui.QColor(53,53,53))
                pal.setColor(QtGui.QPalette.ColorRole.ToolTipBase, QtCore.Qt.GlobalColor.white)
                pal.setColor(QtGui.QPalette.ColorRole.ToolTipText, QtCore.Qt.GlobalColor.white)
                pal.setColor(QtGui.QPalette.ColorRole.Text, QtCore.Qt.GlobalColor.white)
                pal.setColor(QtGui.QPalette.ColorRole.Button, QtGui.QColor(53,53,53))
                pal.setColor(QtGui.QPalette.ColorRole.ButtonText, QtCore.Qt.GlobalColor.white)
                pal.setColor(QtGui.QPalette.ColorRole.BrightText, QtCore.Qt.GlobalColor.red)
                pal.setColor(QtGui.QPalette.ColorRole.Highlight, QtGui.QColor(142,45,197).lighter())
                pal.setColor(QtGui.QPalette.ColorRole.HighlightedText, QtCore.Qt.GlobalColor.black)
                app.setPalette(pal)
                self.btn_theme.setText(t("Light Theme"))
            else:
                app.setStyle("Fusion")
                app.setPalette(app.style().standardPalette())
                self.btn_theme.setText(t("Dark Theme"))

        def _toggle_theme(self):
            self._dark = not self._dark
            self._apply_theme()
            self._dirty = True

        def _toggle_lang(self):
            global LANG
            LANG = "en" if LANG == "de" else "de"
            # retitle tabs, buttons
            self.tabs.setTabText(self.tabs.indexOf(self.dashboard), t("Dashboard"))
            self.tabs.setTabText(self.tabs.indexOf(self.overview), t("Overview"))
            self.act_detect.setText(t("Detect & Calibrate"))
            self.btn_theme.setText(t("Light Theme") if self._dark else t("Dark Theme"))
            self.dashboard.retranslate()
            self._dirty = True

        # ----- Build from detection/config -----
        def _build_from_config(self, cfg: dict):
            # Sensors from current system; we map by label matches
            temps = discover_temp_sensors()
            for s in temps:
                label = s['label']
                self.sensors[label] = SysfsHwmonTemp(label, s['path'])
                self.sensor_types[label] = s.get('type', 'Unknown')
            # outputs from config
            # if none -> run auto setup
            profs = cfg.get("profiles", {})
            if not profs:
                self._auto_setup(); return
            for name, arr in profs.items():
                channels: List[Channel] = []
                for c in arr:
                    s_lbl = c["sensor"]; sensor = self.sensors.get(s_lbl)
                    out = c["output"]
                    output = SysfsPwmOutput(out["label"], out["pwm"], out.get("enable"), out.get("min_pct",0.0), out.get("max_pct",100.0), writable=out.get("writable", True))
                    curve = Curve(points=[tuple(p) for p in c.get("curve", [(20,20),(40,40),(60,60),(80,100)])])
                    ch = Channel(c["name"], sensor if sensor else SysfsHwmonTemp(s_lbl, ""), output, curve,
                                 hysteresis_c=c.get("hyst",0.0), response_tau_s=c.get("tau",0.0), mode=c.get("mode","Auto"))
                    ch.manual_value = float(c.get("manual", 0.0))
                    channels.append(ch)
                self.profile_mgr.add_profile(name, channels)
            if not self.profile_mgr.names():
                self.profile_mgr.add_profile("Default", [])
            self.profile_combo.blockSignals(True)
            self.profile_combo.clear(); self.profile_combo.addItems(self.profile_mgr.names())
            self.profile_combo.setCurrentText(self.profile_mgr.current_name() or self.profile_mgr.names()[0])
            self.profile_combo.blockSignals(False)
            self._render_current_profile()

        def _auto_setup(self):
            dlg = AutoSetupDialog(self)
            result = dlg.exec_and_wait()
            if not result:
                return
            temps = result['sensors']; pwms = result['pwms']; cal_results = result['mapping']  # mapping used for sensor link
            # Register sensors
            for s in temps:
                label = s['label']
                if label not in self.sensors:
                    obj = SysfsHwmonTemp(label, s['path'])
                    self.sensors[label] = obj
                    self.sensor_types[label] = s.get('type', 'Unknown')
            # Register outputs (min pct later adjustable)
            for d in pwms:
                lbl = d['label']
                if lbl not in self.outputs:
                    self.outputs[lbl] = SysfsPwmOutput(lbl, d['pwm_path'], d.get('enable_path'), writable=probe_pwm_writable(d['pwm_path'], d.get('enable_path'))[0])
            # Selection
            sel = DetectCalibrateDialog(self)
            sel.populate_from(pwms, result['mapping'])
            if sel.exec() != QtWidgets.QDialog.DialogCode.Accepted:
                return
            selected = sel.selected()
            # Build default profile
            channels: List[Channel] = []
            for dev in selected:
                out_lbl = dev['label']; s_lbl = dev['sensor_label']
                sensor = self.sensors.get(s_lbl); output = self.outputs.get(out_lbl)
                if not sensor or not output:
                    continue
                default_curve = Curve(points=[(20, 0), (35, 25), (50, 50), (70, 80)])
                ch = Channel(out_lbl, sensor, output, default_curve, hysteresis_c=1.0, response_tau_s=2.0)
                channels.append(ch)
            self.profile_mgr = ProfileManager()
            self.profile_mgr.add_profile('Default', channels)
            self.profile_combo.blockSignals(True)
            self.profile_combo.clear(); self.profile_combo.addItems(self.profile_mgr.names())
            self.profile_combo.setCurrentText('Default')
            self.profile_combo.blockSignals(False)
            self._render_current_profile()
            self._dirty = True

        def _on_profile_switch(self, name: str):
            if not name:
                return
            self.profile_mgr.set_current(name)
            self._render_current_profile()

        def _render_current_profile(self):
            # Close additional tabs (editors)
            while self.tabs.count() > 2:
                self.tabs.removeTab(2)
            # hook outputChanged so both Overview and Dashboard update live
            channels = self.profile_mgr.get_channels()
            for ch in channels:
                def _make_on_out(c=ch):
                    return lambda duty: (self.overview.update_channel_output(c, duty),
                                         self.dashboard.update_output(c, duty))
                ch.outputChanged = _make_on_out()
            self.dashboard.rebuild(channels, self.triggers)
            self.overview.set_hidden(self._hidden_sensors)
            self.overview.rebuild_sensors(list(self.sensors.values()), self.sensor_types)
            self.overview.rebuild_channels(channels)
            # rebuild status bar labels
            for w in self.status_bar_labels.values():
                self.statusBar().removeWidget(w)
            self.status_bar_labels.clear()
            for sname in self._statusbar_show:
                if sname in self.sensors:
                    lbl = QtWidgets.QLabel(f"{sname}: --")
                    lbl.setMinimumWidth(140)
                    self.statusBar().addPermanentWidget(lbl)
                    self.status_bar_labels[sname] = lbl

        def _open_channel_tab(self, ch: Channel):
            for i in range(2, self.tabs.count()):
                w = self.tabs.widget(i)
                if isinstance(w, ChannelDialog) and w.channel is ch:
                    self.tabs.setCurrentIndex(i); return
            w = ChannelDialog(ch, self)
            idx = self.tabs.addTab(w, ch.name)
            self.tabs.setCurrentIndex(idx)

        def _rebuild_status_menu(self):
            self._status_menu.clear()
            for s in sorted(self.sensors.keys()):
                act = self._status_menu.addAction(s)
                act.setCheckable(True)
                act.setChecked(s in self._statusbar_show)
                def toggle(checked, name=s):
                    if checked: self._statusbar_show.add(name)
                    else: self._statusbar_show.discard(name)
                    self._render_current_profile()
                    self._dirty = True
                act.toggled.connect(toggle)

        def _add_profile(self):
            name, ok = QtWidgets.QInputDialog.getText(self, t("Add Profile"), t("Label")+":", text=f"Profile{len(self.profile_mgr.names())+1}")
            if not ok or not name: return
            self.profile_mgr.add_profile(name, [])
            self.profile_combo.addItem(name)
            self.profile_combo.setCurrentText(name)
            self._dirty = True

        def _del_profile(self):
            name = self.profile_mgr.current_name()
            if not name: return
            if len(self.profile_mgr.names()) <= 1: return
            idx = self.profile_combo.currentIndex()
            self.profile_combo.removeItem(idx)
            self.profile_mgr._profiles.pop(name, None)
            self.profile_mgr.set_current(self.profile_mgr.names()[0])
            self._render_current_profile()
            self._dirty = True

        def _tick(self):
            # poll sensors
            for s in self.sensors.values():
                val = s.read()
                if val is not None:
                    if s.name in self.status_bar_labels:
                        self.status_bar_labels[s.name].setText(f"{s.name}: {val:.1f}{s.unit}")
                    self.overview.update_sensor_value(s.name, f"{val:.1f}{s.unit}")
            # apply channels
            for ch in self.profile_mgr.get_channels():
                ch.apply()

        def closeEvent(self, ev):
            if self._dirty:
                m = QtWidgets.QMessageBox(self)
                m.setWindowTitle(t("Save changes?"))
                m.setText(t("You have unsaved changes. Save before exit?"))
                m.setStandardButtons(QtWidgets.QMessageBox.StandardButton.Save | QtWidgets.QMessageBox.StandardButton.Discard | QtWidgets.QMessageBox.StandardButton.Cancel)
                r = m.exec()
                if r == QtWidgets.QMessageBox.StandardButton.Save:
                    self._save_config(); ev.accept()
                elif r == QtWidgets.QMessageBox.StandardButton.Discard:
                    ev.accept()
                else:
                    ev.ignore()
            else:
                ev.accept()

# -----------------------
# CLI / Selftest
# -----------------------
def run_self_tests():
    print("Running self tests…")
    # Curve
    c = Curve(points=[(0, 0), (50, 50), (100, 100)])
    assert c.eval(-10) == 0
    assert c.eval(0) == 0
    assert c.eval(25) == 25
    assert c.eval(50) == 50
    assert c.eval(75) == 75
    assert c.eval(120) == 100
    # Schmitt
    filt = SchmittWithSlew(hysteresis_c=2.0, tau_s=0.0)
    y1 = filt.step(c, 40.0)
    y2 = filt.step(c, 41.0)
    assert 40 <= y1 <= 41
    assert 41 <= y2 <= 43
    # hwmon fake tree
    tmp = tempfile.mkdtemp()
    try:
        base = os.path.join(tmp, 'hwmon0')
        os.makedirs(base, exist_ok=True)
        with open(os.path.join(base, 'name'), 'w') as f: f.write('k10temp')
        for fn, content in [
            ('temp1_input', '42000'),
            ('temp1_label', 'CPU Tctl'),
            ('pwm1', '128'),
            ('pwm1_enable', '1'),
            ('fan1_input', '900'),
        ]:
            with open(os.path.join(base, fn), 'w') as f: f.write(content)
        tree = list_hwmon_tree(os.path.dirname(base))
        assert any('pwm1' in v for v in tree.values())
        devs = discover_pwm_devices(os.path.dirname(base))
        assert len(devs) == 1 and devs[0]['fan_input_path'] is not None
        temps = discover_temp_sensors(os.path.dirname(base))
        assert len(temps) == 1 and 'CPU' in temps[0]['label'] and 'CPU Tctl' in temps[0]['label'] and temps[0]['type'] == 'CPU'
        mapping = couple_outputs_to_sensors(temps, devs)
        assert devs[0]['label'] in mapping
    finally:
        shutil.rmtree(tmp)
    # Active coupling detection (simulated)
    bases = {"/t/cpu": 60.0, "/t/gpu": 70.0}
    kmap  = {"pwmA": {"/t/cpu": 0.02, "/t/gpu": 0.00}, "pwmB": {"/t/cpu": 0.00, "/t/gpu": 0.03}}
    pwm_state = {"/p/A": 50.0, "/p/B": 50.0}
    def io_read_temp(path: str) -> float:
        cpu_eff = 100 - pwm_state["/p/A"]
        gpu_eff = 100 - pwm_state["/p/B"]
        if path == "/t/cpu":
            return bases[path] + kmap["pwmA"][path] * cpu_eff
        if path == "/t/gpu":
            return bases[path] + kmap["pwmB"][path] * gpu_eff
        return 0.0
    def io_read_pwm_raw(path: str) -> str:
        pct = pwm_state[path]
        return str(int(round(pct * 255 / 100)))
    def io_write_pwm_pct(path: str, pct: float):
        pwm_state[path] = clamp(pct, 0.0, 100.0)
    def io_read_enable(_path: Optional[str]) -> Optional[str]:
        return '1'
    def io_write_enable(_path: Optional[str], _val: Optional[str]):
        pass
    mapping = infer_coupling_via_perturbation(
        pwms=[{"label": "A", "pwm_path": "/p/A", "enable_path": None}, {"label": "B", "pwm_path": "/p/B", "enable_path": None}],
        temps=[{"label": "CPU: Tctl", "path": "/t/cpu"}, {"label": "GPU: Hotspot", "path": "/t/gpu"}],
        hold_s=1.0, hold_pct=100, floor_pct=20,
        progress=None, cancelled=None,
        read_temp=io_read_temp, read_pwm_raw=io_read_pwm_raw, write_pwm_pct=io_write_pwm_pct,
        read_enable=io_read_enable, write_enable=io_write_enable, sleep=lambda s: None,
    )
    assert mapping['A']['sensor_label'].startswith('CPU')
    assert mapping['B']['sensor_label'].startswith('GPU')
    print("All tests passed.")

def run_headless_detect():
    temps = discover_temp_sensors()
    pwms = discover_pwm_devices()
    print(f"Detected {len(temps)} sensors, {len(pwms)} PWM outputs.")
    print("Inferring coupling (100% / 10s)…")
    mapping_active = infer_coupling_via_perturbation(
        pwms, temps, hold_s=10.0, hold_pct=100, floor_pct=20,
        progress=lambda s: print("  ", s))
    mapping_heur = couple_outputs_to_sensors(temps, pwms)
    mapping: Dict[str, Any] = dict(mapping_active)
    for k, v in mapping_heur.items():
        mapping.setdefault(k, v)
    for p in pwms:
        lbl = p['label']
        rec = mapping.get(lbl, {})
        print(f"PWM {lbl} -> {rec.get('sensor_label', 'n/a')}")

def run_headless_calibrate():
    pwms = discover_pwm_devices()
    cal = Calibrator()
    snapshots = {d['label']: {"enable": read_text(d.get('enable_path')), "pwm": read_text(d.get('pwm_path'))} for d in pwms}
    def restore_all():
        for d in pwms:
            snap = snapshots.get(d['label'], {})
            write_text(d.get('enable_path'), snap.get('enable'))
            write_text(d.get('pwm_path'), snap.get('pwm'))
    try:
        print(f"Calibrating {len(pwms)} PWM outputs…")
        results = cal.calibrate_all(pwms, floor_pct=20, progress=lambda i,t,l: print(f"  {i}/{t} {l}"))
        for k, v in results.get('results', {}).items():
            print(" ", k, "->", v)
    finally:
        restore_all()
        print("Restored previous PWM states.")

def main(argv: Optional[List[str]] = None):
    parser = argparse.ArgumentParser(description=APP_TITLE)
    parser.add_argument("--detect", action="store_true", help="Headless: detect/couple sensors and outputs")
    parser.add_argument("--calibrate", action="store_true", help="Headless: calibrate all PWMs (safe floor, snapshot+restore)")
    parser.add_argument("--selftest", action="store_true", help="Run built-in tests")
    args = parser.parse_args(argv)

    if args.selftest:
        run_self_tests()
        return

    if args.detect:
        run_headless_detect()
        return

    if args.calibrate:
        run_headless_calibrate()
        return

    # GUI path if available; fallback to headless detect
    if GUI_AVAILABLE:
        app = QtWidgets.QApplication(sys.argv)  # type: ignore
        win = MainWindow(config=None)  # type: ignore
        win.show()
        sys.exit(app.exec())
    else:
        print("[INFO] GUI not available. Running headless --detect.")
        run_headless_detect()

if __name__ == "__main__":
    main()

# 🇩🇪 ENV Variablen / 🇬🇧 ENV Variables

> **Gültigkeit / Validity:** ENV‑Variablen wirken als **Fallback**. Reihenfolge: **Defaults → ENV → `daemon.json`**. D. h. Eintrag in `daemon.json` überschreibt gesetzte ENV.
> **Order:** ENV variables act as **fallback**. Sequence: **Defaults → ENV → `daemon.json`**. That means a `daemon.json` entry overrides any ENV setting.

---

## 🇩🇪 Laufzeittakt & Reaktivität (Daemon) / 🇬🇧 Runtime cadence & responsiveness (Daemon)

Diese Parameter sind auch in `daemon.json` konfigurierbar und via RPC `config.set` zur Laufzeit änderbar.
These parameters are also configurable in `daemon.json` and adjustable at runtime via RPC `config.set`.

| ENV                     | Standard / Default | Bereich / Range | Wirkung / Effect                                                                                                                                                                                      |                                                                                                                               |
| ----------------------- | ------------------ | --------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------- |
| `LFCD_TICK_MS`          | `50`               | `5..1000` ms    | 🇩🇪 Takt des Hauptloops (Engine‑Tick). Höher = weniger CPU‑Last, träger; niedriger = reaktiver.  /  🇬🇧 Main loop tick interval. Higher = less CPU load but slower response; lower = more reactive. |                                                                                                                               |
| `LFCD_DELTA_C`          | `0.7`              | `0.0..10.0` °C  | 🇩🇪 Schwellwert: Engine tick nur bei Temperaturänderung ≥ ΔC.  /  🇬🇧 Threshold: engine tick only runs when a sensor changes ≥ ΔC. |                                     
| `LFCD_FORCE_TICK_MS`    | `2000`             | `100..10000` ms | 🇩🇪 Spätestens alle X ms erzwungener Publish/Tick (`0` = aus).  /  🇬🇧 Forced publish/tick at least every X ms (`0` = off). |
| `LFCD_GPU_REFRESH_MS`   | `1000`             | `100..60000` ms | 🇩🇪 Intervall für leichten GPU‑Refresh. `0` = aus.  /  🇬🇧 Interval for lightweight GPU refresh. `0` = disabled.            |
| `LFCD_HWMON_REFRESH_MS` | `500`              | `100..60000` ms | 🇩🇪 Intervall für leichtes hwmon‑Refresh. `0` = aus.  /  🇬🇧 Interval for lightweight hwmon refresh. `0` = disabled.        |

---

## 🇩🇪 Vendor‑Mapping (Daemon) / 🇬🇧 Vendor mapping (Daemon)

| ENV                          | Standard / Default | Wirkung / Effect                                                                              |
| ---------------------------- | ------------------ | --------------------------------------------------------------------------------------------- |
| `LFC_VENDOR_MAP`             | —                  | 🇩🇪 Absoluter Pfad zur `vendorMapping.json`.  /  🇬🇧 Absolute path to `vendorMapping.json`. |
| `LFC_VENDOR_MAP_WATCH`       | `mtime`            | 🇩🇪 Watch‑Modus (`mtime` Poll).  /  🇬🇧 Watch mode (`mtime` polling).                       |
| `LFC_VENDOR_MAP_THROTTLE_MS` | `3000`             | 🇩🇪 Drossel für Polling.  /  🇬🇧 Throttle for polling.                                      |

**Hinweise / Notes**
🇩🇪 Ohne `LFC_VENDOR_MAP` sucht der Daemon in Standardpfaden (`~/.config/...`, `/etc/...`, `/usr/share/...`).
🇬🇧 Without `LFC_VENDOR_MAP`, the daemon searches standard paths (`~/.config/...`, `/etc/...`, `/usr/share/...`).

---

## 🇩🇪 XDG‑Pfadvariablen / 🇬🇧 XDG path variables

| ENV               | Verwendung / Usage                                                                                 |
| ----------------- | -------------------------------------------------------------------------------------------------- |
| `XDG_CONFIG_HOME` | 🇩🇪 Basis für `~/.config` → Config + Profiles.  /  🇬🇧 Base for `~/.config` → config + profiles. |
| `XDG_STATE_HOME`  | 🇩🇪 Derzeit nur intern.  /  🇬🇧 Currently internal only.                                         |
| `XDG_RUNTIME_DIR` | 🇩🇪 Laufzeitpfade; SHM nutzt POSIX‑Namen.  /  🇬🇧 Runtime paths; SHM uses POSIX names.           |

> 🇩🇪 `shmPath` ist ein **Name** (z. B. `"lfc.telemetry"`) und wird zu `"/lfc.telemetry"` normalisiert.
> 🇬🇧 `shmPath` is a **name** (e.g. `"lfc.telemetry"`) and normalized to `"/lfc.telemetry"`.

---

## 🇩🇪 Laufzeit‑Helper & GUI‑Start / 🇬🇧 Runtime helpers & GUI start

| ENV                | Standard / Default  | Wirkung / Effect                                                   |
| ------------------ | ------------------- | ------------------------------------------------------------------ |
| `LFCD_DAEMON`      | autodetect          | 🇩🇪 Pfad zum Daemon‑Binary.  /  🇬🇧 Path to daemon binary.       |
| `LFCD_DAEMON_ARGS` | `""` (leer / empty) | 🇩🇪 Zusätzliche CLI‑Argumente.  /  🇬🇧 Additional CLI arguments. |

---

## 🇩🇪 Build‑Skripte (build.sh) / 🇬🇧 Build scripts (build.sh)

| ENV           | Standard / Default | Wirkung / Effect                                                       |
| ------------- | ------------------ | ---------------------------------------------------------------------- |
| `FRESH`       | `0`                | 🇩🇪 `1` = Clean Build.  /  🇬🇧 `1` = Clean build.                    |
| `FIRST_ERROR` | `0`                | 🇩🇪 `1` = Stop bei erstem Fehler.  /  🇬🇧 `1` = Stop at first error. |
| `JOBS`        | auto (CPUs)        | 🇩🇪 Anzahl paralleler Jobs.  /  🇬🇧 Parallel jobs count.             |
| `CC`/`CXX`    | toolchain default  | 🇩🇪 Compiler überschreiben.  /  🇬🇧 Override compiler.               |

---

## 🇩🇪 Build‑Skript (build2.sh) / 🇬🇧 Build script (build2.sh)

| ENV          | Standard / Default | Wirkung / Effect                                         |
| ------------ | ------------------ | -------------------------------------------------------- |
| `GUI_CONFIG` | `Release`          | 🇩🇪 Build‑Konfiguration.  /  🇬🇧 Build configuration.  |
| `GUI_RID`    | `linux-x64`        | 🇩🇪 .NET RID.  /  🇬🇧 .NET RID.                        |
| `ARTIFACTS`  | `artifacts`        | 🇩🇪 Ausgabeverzeichnis.  /  🇬🇧 Output directory.      |
| `BUILD_DIR`  | `build`            | 🇩🇪 Build‑Zwischenstände.  /  🇬🇧 Build intermediates. |

---

## 🇩🇪 Import‑Tool (scripts/importFC.sh) / 🇬🇧 Import tool (scripts/importFC.sh)

🇩🇪 Keine ENV‑Schalter; Steuerung per CLI‑Optionen.
🇬🇧 No ENV switches; controlled via CLI options.

---

## 🇩🇪 lfc\_live.py / 🇬🇧 lfc\_live.py

🇩🇪 Keine eigenen ENV‑Variablen. SHM‑Quelle per `--shm` wählbar.
🇬🇧 No own ENV variables. SHM source selectable via `--shm`.

---

## 🇩🇪 Quick‑Referenz / 🇬🇧 Quick reference

```bash
# 🇩🇪 Daemon mit konservativerer Last
# 🇬🇧 Run daemon with more conservative load
LFCD_TICK_MS=40 LFCD_FORCE_TICK_MS=1500 LFCD_DELTA_C=0.8 ./lfcd

# 🇩🇪 Refresh‑Kadenzen ohne Rebuild justieren
# 🇬🇧 Adjust refresh cadence without rebuild
LFCD_GPU_REFRESH_MS=1500 LFCD_HWMON_REFRESH_MS=750 ./lfcd

# 🇩🇪 Vendor‑Mapping explizit setzen
# 🇬🇧 Explicitly set vendor mapping
LFC_VENDOR_MAP="$HOME/vendorMapping.json" ./lfcd
```

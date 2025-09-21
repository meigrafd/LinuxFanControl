## 🇩🇪 ENV Variablen

### Laufzeittakt & Reaktivität

Alle drei sind auch über `daemon.json` steuerbar (`tickMs`, `deltaC`, `forceTickMs`) und via RPC `config.set` zur Laufzeit änderbar.

| ENV                      | Standard | Bereich         | Wirkung                                                                                               |
| ------------------------ | -------- | --------------- | ----------------------------------------------------------------------------------------------------- |
| **`LFCD_TICK_MS`**       | `25`     | `5..1000` ms    | Takt des Hauptloops (Engine‑Tick). Höher = weniger CPU, träger; niedriger = reaktiver.                |
| **`LFCD_DELTA_C`**       | `0.5`    | `0.0..10.0` °C  | Schwelle: `engine.tick()` läuft nur, wenn sich ein Sensor ≥ ΔC verändert hat (Detection ausgenommen). |
| **`LFCD_FORCE_TICK_MS`** | `1000`   | `100..10000` ms | Spätestens alle `forceTickMs` wird ein Tick erzwungen, selbst ohne Temp‑Änderung.                     |

### Vendor Mapping

| ENV                  | Standard | Wirkung                                                                                                                                             |
| -------------------- | -------- | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| **`LFC_VENDOR_MAP`** | —        | Absoluter Pfad zur `vendorMapping.json`. Setzt einen **expliziten Override**; bei gesetztem Override gibt es **keinen** Fallback auf Default‑Pfade. |

### Sonstige Pfade (XDG)

`lfcd` nutzt XDG‑Konventionen:

* **Config**: `XDG_CONFIG_HOME` (fallback `~/.config`), Datei `LinuxFanControl/daemon.json`.
* **State/Logs**: `XDG_STATE_HOME` (fallback `~/.local/state`), z. B. `LinuxFanControl/daemon_lfc.log`.
* **Runtime**: `XDG_RUNTIME_DIR` (nur für systemische Defaults; Telemetry nutzt POSIX‑SHM, siehe unten).

> **Telemetry (SHM)**: `daemon.json`‑Schlüssel `shmPath` ist der **SHM‑Name** (Standard: `"lfc.telemetry"`, normalisiert zu `"/lfc.telemetry"`). Fallback auf Datei erfolgt nur, wenn SHM‑Erzeugung fehlschlägt.

---

## 🇬🇧 ENV's

#### Loop Cadence & Reactivity

All three also exist in `daemon.json` (`tickMs`, `deltaC`, `forceTickMs`) and are changeable at runtime via `config.set`.

| ENV                      | Default | Range           | Effect                                                                                 |
| ------------------------ | ------- | --------------- | -------------------------------------------------------------------------------------- |
| **`LFCD_TICK_MS`**       | `25`    | `5..1000` ms    | Main loop cadence (engine ticks). Higher = lower CPU, slower; lower = more responsive. |
| **`LFCD_DELTA_C`**       | `0.5`   | `0.0..10.0` °C  | Temperature delta required to run a non‑forced tick (detection excluded).              |
| **`LFCD_FORCE_TICK_MS`** | `1000`  | `100..10000` ms | Force a tick at least every `forceTickMs` even without temp change.                    |

#### Vendor Mapping

| ENV                  | Default | Effect                                                                                                             |
| -------------------- | ------- | ------------------------------------------------------------------------------------------------------------------ |
| **`LFC_VENDOR_MAP`** | —       | Absolute path to `vendorMapping.json`. Sets a **strict override**; when set, no default‑path probing is performed. |

#### Other Paths (XDG)

`lfcd` follows XDG:

* **Config**: `XDG_CONFIG_HOME` (fallback `~/.config`), file `LinuxFanControl/daemon.json`.
* **State/Logs**: `XDG_STATE_HOME` (fallback `~/.local/state`), e.g., `LinuxFanControl/daemon_lfc.log`.
* **Runtime**: `XDG_RUNTIME_DIR` (system defaults only; telemetry uses POSIX SHM, see below).

> **Telemetry (SHM)**: `daemon.json` key `shmPath` is the **SHM name** (default `"lfc.telemetry"`, normalized to `"/lfc.telemetry"`). File fallback is used only if SHM creation fails.

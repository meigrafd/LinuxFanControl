# ShmTelemetry — Struktur & Nutzung / Structure & Usage

> **DE/EN Parallel**: Jede Aussage steht zuerst **Deutsch**, darunter **Englisch**.

🇩🇪 Diese Seite beschreibt das **Shared‑Memory Telemetrie‑Format** des Daemons (lfcd). Ziel ist eine stabile, leicht konsumierbare JSON‑Repräsentation aktueller Sensordaten, die periodisch in einen POSIX‑SHM‑Block geschrieben wird.

🇬🇧 This page describes the **shared‑memory telemetry format** of the daemon (lfcd). The goal is a stable, easily consumable JSON representation of current sensor data, written periodically into a POSIX SHM block.

---

## Zweck & Eigenschaften / Purpose & Properties

* **DE:** Read‑only für Clients; nur der Daemon schreibt. Niedrige Latenz (Engine‑Tick‑gesteuert), portable JSON‑Darstellung im SHM‑File unter `/dev/shm`, vorwärts‑kompatibel (Felder werden nur ergänzt).
* **EN:** Read‑only for clients; only the daemon writes. Low‑latency (engine‑tick‑driven), portable JSON in an SHM‑backed file under `/dev/shm`, forward‑compatible (fields are only added).

---

## Speicherort & Benennung / Location & Naming

* **DE:** Der SHM‑Name wird in `daemon.json` konfiguriert (z. B. `lfc.telemetry`) und zu einem POSIX‑Namen **mit** führendem Slash normalisiert: `"/lfc.telemetry"`. Das Backing‑File liegt unter `/dev/shm/lfc.telemetry`.
* **EN:** The SHM name is configured in `daemon.json` (e.g., `lfc.telemetry`) and normalized to a POSIX name **with** a leading slash: `"/lfc.telemetry"`. The backing file resides at `/dev/shm/lfc.telemetry`.

---

## Aktualisierungs‑Semantik / Update Semantics

* **DE:** Schreibintervall über `tickMs`; Δ‑Schwellwert `deltaC` reduziert Updates; spätestens alle `forceTickMs` wird ein Tick erzwungen. Der Daemon schreibt stets einen konsistenten JSON‑Block.
* **EN:** Write interval via `tickMs`; delta threshold `deltaC` reduces updates; a tick is forced at least every `forceTickMs`. The daemon always writes a consistent JSON block.

---

## Top‑Level Schema (JSON)

```json
{
  "version": "0.3.0",
  "timestampMs": 1690000000000,
  "engineEnabled": true,
  "tickMs": 25,
  "deltaC": 0.5,
  "forceTickMs": 1000,

  "profile": { /* siehe unten / see below */ },
  "hwmon": { /* siehe unten / see below */ },
  "gpus": [ /* siehe unten / see below */ ]
}
```

### Felder / Fields

* **DE:** `version`, `timestampMs`, `engineEnabled`, `tickMs`, `deltaC`, `forceTickMs`, `profile`, `hwmon`, `gpus`.
* **EN:** `version`, `timestampMs`, `engineEnabled`, `tickMs`, `deltaC`, `forceTickMs`, `profile`, `hwmon`, `gpus`.

---

## Profil‑Metadaten / Profile Metadata

```json
{
  "name": "MyProfile",
  "schema": 1,
  "description": "...",
  "curveCount": 8,
  "controlCount": 6
}
```

* **DE:** Metadaten des aktiven Profils (falls vorhanden).
* **EN:** Metadata of the active profile (if present).

---

## HWMon — Chips/Temps/Fans/PWMs

### `HwmonChip`

```json
{ "name":"nct6799d", "vendor":"Nuvoton", "hwmonPath":"/sys/class/hwmon/hwmon8", "aliases":["nuvoton","nct67"], "driver":"nct6775" }
```

* **DE:** `name`, `vendor` (VendorMapping), `hwmonPath`, optionale `aliases`, `driver`.
* **EN:** `name`, `vendor` (VendorMapping), `hwmonPath`, optional `aliases`, `driver`.

### `HwmonTemp`

```json
{ "chipPath":"/sys/class/hwmon/hwmon8", "path":"/sys/class/hwmon/hwmon8/temp1_input", "label":"CPU", "valueC":38.2 }
```

* **DE:** Temperatur in °C unter `valueC` (falls verfügbar).
* **EN:** Temperature in °C under `valueC` (if available).

### `HwmonFan`

```json
{ "chipPath":"/sys/class/hwmon/hwmon8", "path":"/sys/class/hwmon/hwmon8/fan1_input", "label":"CHA1", "rpm":920 }
```

* **DE:** Tachosignal in `rpm` (optional).
* **EN:** Tach signal in `rpm` (optional).

### `HwmonPwm`

```json
{ "chipPath":"/sys/class/hwmon/hwmon8", "path":"/sys/class/hwmon/hwmon8/pwm1", "pathEnable":"/sys/class/hwmon/hwmon8/pwm1_enable", "pwmMax":255, "label":"CHA1", "value":102, "percent":40, "enable":1, "rpm":920 }
```

* **DE:** `value` (raw), `percent` (0–100, falls berechnet), `enable` (roh), `rpm` (optional). Mapping: `0→AUTO`, `1|2→MAN`, `3|4|5→HW`, `null→—`, ohne `pathEnable→N/A`.
* **EN:** `value` (raw), `percent` (0–100, if computed), `enable` (raw), `rpm` (optional). Mapping: `0→AUTO`, `1|2→MAN`, `3|4|5→HW`, `null→—`, without `pathEnable→N/A`.

---

## GPU Telemetrie / GPU Telemetry (`GpuInfo`)

```json
{ "vendor":"AMD", "name":"Radeon RX 7900 XT", "index":0, "pci":"0000:03:00.0", "drm":"card0", "hwmon":"/sys/class/hwmon/hwmon4",
  "tempEdgeC":45.0, "tempHotspotC":57.5, "tempMemoryC":66.8,
  "fanRpm":980, "fanPercent":35, "hasFanTach":true, "hasFanPwm":true }
```

* **DE:** Identifikation über `vendor`/`name`/`index`/`pci`/`drm`/`hwmon`; Temperaturen `tempEdgeC`/`tempHotspotC`/`tempMemoryC`; Lüfter `fanRpm`/`fanPercent`; Fähigkeitsbits.
* **EN:** Identification via `vendor`/`name`/`index`/`pci`/`drm`/`hwmon`; temperatures `tempEdgeC`/`tempHotspotC`/`tempMemoryC`; fans `fanRpm`/`fanPercent`; capability bits.

**DE:** Quellen: AMDSMI (AMD), NVML (NVIDIA), IGCL/Level‑Zero (Intel) mit hwmon‑Fallback.
**EN:** Sources: AMDSMI (AMD), NVML (NVIDIA), IGCL/Level‑Zero (Intel) with hwmon fallback.

---

## Kompatibilität / Compatibility

* **DE:** Schema wächst additiv; Unbekanntes ignorieren; optionale Felder prüfen.
* **EN:** Schema evolves additively; ignore unknowns; check optional fields.

---

## Fehlerbehandlung / Error Handling

* **DE:** Parse‑Fehler → erneut lesen; fehlende Datei → Daemon/Name prüfen; `null`/fehlend → als nicht verfügbar anzeigen.
* **EN:** Parse errors → re‑read; missing file → check daemon/name; `null`/missing → display as not available.

---

## Performance‑Hinweise / Performance Notes

* **DE:** Periodisch vollständig lesen (z. B. 1 Hz); keine In‑Place‑Mutationen. SHM‑Größe ≥ 128 KiB empfohlen.
* **EN:** Read fully at a periodic interval (e.g., 1 Hz); no in‑place mutations. SHM size ≥ 128 KiB recommended.

---

## Beispiel‑Client / Example Client (Python)

```python
import json, os
path = "/dev/shm/lfc.telemetry"
raw  = open(path, "rb").read().rstrip(b"\x00")
doc  = json.loads(raw.decode("utf-8", "ignore"))
print(doc.get("version"), doc.get("engineEnabled"))
```

---

## Best Practices / Best Practices

* **DE:** Prozent nur anzeigen, wenn vorhanden; sonst RAW‑PWM. GPU‑Werte bevorzugt aus Vendor‑API. Fehlende Werte mit `—`.
* **EN:** Show percent only if present; otherwise raw PWM. Prefer vendor APIs for GPU values. Use `—` for missing values.

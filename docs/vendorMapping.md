## 🇩🇪 Vendor Mapping

### Ziel

`vendorMapping.json` erlaubt es, `hwmon`/Chip‑Namen zur Laufzeit Klassen & Vendor‑Labels zuzuordnen (z. B. **CPU vs. GPU vs. CHIPSET**), ohne Rebuild. Die Datei wird beim Start geladen und je nach Modus automatisch neu eingelesen.

### Dateisuche & Override

1. Wenn `vendorMapPath` in `daemon.json` gesetzt ist → **nur** diese Datei wird genutzt (kein Fallback).
2. Sonst, wenn `LFC_VENDOR_MAP` gesetzt ist → diese Datei.
3. Sonst Default‑Pfade (in Reihenfolge):

   * `~/.config/LinuxFanControl/vendorMapping.json`
   * `/etc/LinuxFanControl/vendorMapping.json`
   * `/usr/share/LinuxFanControl/vendorMapping.json`

### Hot‑Reload

* **Watch‑Modus** in `daemon.json` über `vendorMapWatchMode`: `"mtime"` (Polling, Standard) oder `"inotify"` (sofort bei Dateiänderung, Linux‑only).
* Drosselung in ms über `vendorMapThrottleMs` (wirksam im `mtime`‑Modus).

### JSON‑Schema

Jede Regel ist ein Objekt mit:

```json
{
  "regex": "^k10temp",
  "vendor": "AMD CPU (Zen/K10)",
  "class": "CPU",
  "priority": 200,
  "flags": "i"
}
```

* **regex** *(string, erforderlich)* — ECMAScript Regex. Inline `(?i)` wird toleriert und als **case‑insensitive** interpretiert.
* **vendor** *(string, erforderlich)* — frei wählbares Label.
* **class** *(string, optional)* — Gruppierung (z. B. `CPU`, `GPU`, `CHIPSET`, `EC`, `SUPERIO`, `SENSOR`, `STORAGE`, `VRM`, `PSU`, `SOC`, `BMC`, `BOARD`, `MEMORY`, `ACPI`, `VIRTUAL`, `GENERIC`).
* **priority** *(int, optional; Standard `0`)* — Höhere Zahl gewinnt, wenn mehrere Regeln matchen.
* **flags** *(string, optional)* — z. B. `"i"` für case‑insensitive; wird mit `std::regex::icase` umgesetzt.

> **Hinweis:** Bei Konflikten entscheidet zuerst `priority`. Bei Gleichstand gilt die erste passende Regel.

#### Regex‑Flags (`flags`)

* **Unterstützt:** aktuell nur `"i"` für *case‑insensitive* Matching.
* **Inline‑Variante:** Ein führendes `(?i)` am **Anfang** des Patterns wird erkannt, entfernt und als `icase` interpretiert. (Scoped‑Flags wie `(?i:...)` werden **nicht** unterstützt.)
* **Empfehlung:** Verwende `"flags": "i"` statt `(?i)` für Klarheit und Portabilität.
* **Hinweis:** Regex‑Syntax ist **ECMAScript** (`std::regex`).

### Beispiel‑Snippet

```json
[
  { "regex": "^amdgpu$", "vendor": "AMD GPU", "class": "GPU", "priority": 220 },
  { "regex": "^k10temp", "vendor": "AMD CPU (Zen/K10)", "class": "CPU", "priority": 200, "flags": "i" },
  { "regex": "^coretemp$", "vendor": "Intel CPU (CoreTemp)", "class": "CPU", "priority": 200 },
  { "regex": "^nct67(75|76|77|78|79|80|85|86)", "vendor": "Nuvoton Super-IO", "class": "SUPERIO", "priority": 150, "flags": "i" },
  { "regex": "^nvme", "vendor": "NVMe Drive", "class": "STORAGE", "priority": 140 }
]
```
---

## 🇬🇧 Vendor Mapping

#### Purpose

`vendorMapping.json` lets you assign runtime **vendor/class labels** to `hwmon`/chip names (e.g., **CPU vs. GPU vs. CHIPSET**) without recompilation. The file is loaded at startup and reloaded automatically depending on watch mode.

#### File Lookup & Override

1. If `vendorMapPath` is set in `daemon.json` → use **only** this path (no fallback).
2. Else if `LFC_VENDOR_MAP` is set → use that file.
3. Else probe defaults in order:

   * `~/.config/LinuxFanControl/vendorMapping.json`
   * `/etc/LinuxFanControl/vendorMapping.json`
   * `/usr/share/LinuxFanControl/vendorMapping.json`

#### Hot Reload

* Watch mode via `vendorMapWatchMode`: `"mtime"` (polling, default) or `"inotify"` (instant on change, Linux‑only).
* Throttling ms via `vendorMapThrottleMs` (effective in `mtime` mode).

#### JSON Schema (lightweight)

See German section above. Same fields: `regex`, `vendor`, `class`, `priority`, `flags`. ECMAScript regex; leading `(?i)` is tolerated and mapped to `icase`.

#### Example Snippet

See German section above.

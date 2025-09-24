# LinuxFanControl — Daemon Changelog (English)

> This file tracks daemon-side changes only (build system, runtime, telemetry, RPC). GUI changes are tracked elsewhere.

---

## 2025-09-24

### Added

* **GPU detection improvements**: `GpuMonitor` now performs more accurate identification (vendor, board name, PCI mapping). Enhancements integrated with extended `vendorMapping` database.

* **ShmTelemetry guard**: Introduced lightweight guard logic to prevent redundant or excessive SHM writes, reducing contention and CPU overhead.

* **FanControl profile import**: Reworked importer for Windows FanControl.Release profiles (`userConfig.json`), now preserving triggers, mixes, graphs, and nicknames more faithfully.

* **SDK auto-detection** (CMake): NVML and IGCL/Level Zero are now detected automatically, mirroring the existing AMDSMI flow. Auto-enables when headers **and** libs are found; can be hard-disabled via `DISABLE_*`, or forced via `WITH_*`.

* **GPU enrichment backends**:

  * **NVML**: Edge/Hotspot (if exposed), fan RPM/percent, pretty name, PCI BDF mapping.
  * **IGCL/Level Zero (ZES)**: Edge/Hotspot/Memory temps, fan RPM/percent, pretty name, PCI BDF mapping.

* **Runtime-configurable refresh cadences**:

  * `gpuRefreshMs` and `hwmonRefreshMs` in `daemon.json` (with ENV fallbacks `LFCD_GPU_REFRESH_MS`, `LFCD_HWMON_REFRESH_MS`).

* **ENV fallback layer** for daemon configuration (strictly additive):

  * Engine: `LFCD_TICK_MS`, `LFCD_FORCE_TICK_MS`, `LFCD_DELTA_C`, `LFCD_DEBUG`.
  * RPC/Paths: `LFCD_HOST`, `LFCD_PORT`, `LFCD_SHM_PATH` (alias `LFC_SHM_PATH`), `LFCD_LOGFILE`, `LFCD_PIDFILE`, `LFCD_PROFILES_PATH`, `LFCD_PROFILE_NAME`, `LFCD_CONFIG_PATH`.
  * Vendor mapping: `LFC_VENDOR_MAP`, `LFC_VENDOR_MAP_WATCH`, `LFC_VENDOR_MAP_THROTTLE_MS`.

* **Telemetry dump script**: `scripts/lfc_telemetry_dump.py` — dumps SHM telemetry to a text file with pretty-printed JSON. Default output path: `/tmp/lfc_shm.txt`.

* **Documentation**:

  * `docs/ShmTelemetry.md` (bilingual DE/EN) — schema, semantics, client hints.
  * `docs/env.md` (bilingual DE/EN) — full list of supported ENV variables and precedence.

### Changed

* **Daemon run loop**: replaced fixed `sleep_for(10ms)` with a **dynamic sleep** to the earliest next due event (engine tick, forced publish, GPU refresh, hwmon refresh). Responsiveness guardrails: min 1 ms / max 50 ms.
* **`lfc_live.py`** now reads **SHM only** (no direct sysfs reads). Output formatting clarified; SHM name normalization kept.

### Fixed

* **JSON ADL bindings** for `DaemonConfig`: ensured `to_json(json&, const DaemonConfig&)` and `from_json(const json&, DaemonConfig&)` are defined **in `namespace lfc`** with exact signatures. This resolves initializer issues in RPC (e.g., `json{{"config", cfg}}`).
* **Header/implementation cohesion**: kept field defaults in the header; `defaultConfig()` defines platform paths and does not silently alter engine timing fields.

### Performance

* **Lower idle CPU load** via dynamic sleep in the run loop.
* **Optional reduction of I/O**: forced publish cadence remains configurable (`forceTickMs`), and GPU/HWMON refreshes can be tuned without rebuild.

### Migration Notes

* No breaking changes to public names or RPC endpoints.
* To tune refresh cadence without rebuild:

  ```bash
  LFCD_GPU_REFRESH_MS=1500 LFCD_HWMON_REFRESH_MS=750 ./lfcd
  ```
* ENV precedence is **Defaults → ENV → `daemon.json`**. If a key is present in `daemon.json`, it overrides the ENV value.

---

## \[Earlier]

* **\[0.2.x] — Pre‑0.3.0**

  * DRM‑based GPU discovery with hwmon correlation (tachometer & PWM mapping), plus vendor backend hooks wired: AMDSMI, NVML and IGCL (enrichment calls present; activation gated by CMake detection).
  * Introduced lightweight metrics refresh (`GpuMonitor::refreshMetrics`), keeping inventory static during runtime.
  * Implemented `/sys/class/hwmon` scanner with temp/fan/PWM discovery and read/write helpers; added `Hwmon::refreshValues` placeholder (no re‑inventory at runtime).
  * Added logger singleton with leveled output (Error..Trace), optional file logging and rotation; reduced default chatter in hot paths.
  * Brought up RPC TCP server and basic config endpoints (get/set), along with profile‑aware startup in the engine.
  * Initial run loop with fixed `sleep_for(10ms)` and periodic tasks (GPU: 1000 ms, hwmon: 500 ms, forced telemetry publish cadence).
  * Telemetry publisher (JSON over POSIX SHM with file fallback) — initial schema for chips/temps/fans/PWMs/GPUs and compact profile summary.
  * Build scripts for daemon and GUI artifacts (CMake+Ninja for daemon; `dotnet publish` for GUI), plus convenience wrappers.

* **\[0.1.0] — Initial daemon**

  * First public skeleton: engine core, hwmon scanning, basic RPC, profile loading, PID/logfile handling, SHM naming conventions.
  * Baseline configuration defaults and `daemon.json` support established.

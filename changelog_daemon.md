# Changelog

All notable changes to **Linux Fan Control (daemon)** will be documented in this file.

## \[0.3.0] — 2025-09-21

### Highlights

* **One-time hardware discovery**: hwmon inventory is scanned once at startup; runtime now reads only live values. This removes noisy rescans and reduces I/O.
* **GPU monitoring overhaul**: strict de-duplication across DRM/HWMon/Vendor SDKs and lightweight metrics refresh; no periodic re-inventory.
* **POSIX shared memory telemetry**: telemetry is published to a real POSIX SHM object (e.g. `/lfc.telemetry`) with file fallback only on failure.
* **Profile-aware startup**: engine remains disabled until a valid profile from `daemon.json` is loaded successfully.
* **Quieter logs**: repeated informational spam removed or downgraded to TRACE; first-run info is retained.

### Added

* `Hwmon::refreshValues(HwmonInventory&)`: updates values/metadata for discovered sensors and purges vanished nodes **without** rescanning inventory.
* `GpuMonitor::refreshMetrics(std::vector<GpuSample>&)`: updates temps/RPM/PWM/power for known GPUs without re-discovery.
* POSIX SHM implementation in `ShmTelemetry` using `shm_open`/`ftruncate`/`mmap`. Automatic fallback to a regular file if SHM is unavailable.
* Deduplication logic in GPU monitoring with priority **PCI → HWMON path → Vendor+Name**.
* Config defaults ensuring a proper SHM **name**: `shmPath` defaults to `"lfc.telemetry"` (normalized to `"/lfc.telemetry"`).

### Changed

* **Daemon run loop**:

  * Removed periodic hwmon rescans; values are read live and `Hwmon::refreshValues` may run at a coarse cadence for housekeeping.
  * GPU handling split into one-time `snapshot()` at init and periodic `refreshMetrics()` in the loop.
  * Telemetry publishing unchanged in behavior but now targets POSIX SHM by default.
* **Startup activation**: the engine is explicitly **disabled by default** and only enabled after loading the active profile from disk (if present & valid).
* **Logging**:

  * Repeated `INFO` logs like "gpu: snapshot complete" and "Hwmon: scan complete" are now emitted once; subsequent cycles use `TRACE`.
  * Reduced log noise during steady-state polling.

### Fixed

* Duplicate GPU entries when data came from multiple sources (e.g., DRM `cardX`, hwmon, NVML/AMDSMI). The dedup now resolves `cardX` → PCI and merges consistently.
* Erroneous DRM entries with `pci: "cardN"` are normalized to a real PCI bus id (e.g., `0000:03:00.0`).

### Breaking / Potentially Impactful Changes

* **Telemetry transport** now prefers POSIX SHM. Consumers that previously tailed a file under `/run/user/...` should read from the SHM object `/lfc.telemetry`. A file at the path in `daemon.json` is only written if SHM creation fails.
* **Telemetry format** remains JSON, but the location changed (see above). A `version: "1"` field is included in the JSON root for forward compatibility.

### Configuration

* `daemon.json` now expects (or defaults) `"shmPath": "lfc.telemetry"`.
* `profileName` and `profilesDir` are honored at startup: if `<profilesDir>/<profileName>.json` exists and loads successfully, the engine is enabled; otherwise it stays disabled.

### Build & Integration Notes

* On some systems you may need to link `-lrt` for `shm_open`. If the linker complains, add `target_link_libraries(lfcd PRIVATE rt)` to your CMake target.
* Vendor SDKs (NVML/AMDSMI/IGCL) remain **optional**. Define `HAVE_NVML`, `HAVE_AMDSMI`, `HAVE_IGCL` and link their libraries to enable richer metrics. Without them, hwmon/DRM discovery still works.

### Internal Refactors

* `GpuMonitor` helpers consolidated and guarded with feature macros to avoid `-Wunused-function` warnings when vendor SDKs are disabled.
* `Config` defaults modernized; paths are expanded via `~` where applicable; timing parameters clamped to sane ranges.

---

## \[0.2.0] — 2025-08-xx

(Previous release summary)

* Periodic hwmon scans and GPU snapshots in the main loop.
* Telemetry written as a regular JSON file (often under `$XDG_RUNTIME_DIR`).
* Simpler GPU detection with occasional duplicates across information sources.
* Engine activation behavior less strict at startup.

---

## Upgrade Guide: 0.2.0 → 0.3.0

1. **Rebuild the daemon**. If you see `shm_open` link errors, add `rt` to link libraries.
2. **Update `daemon.json`**: set `"shmPath": "lfc.telemetry"` (or another SHM name). Keep `profileName`/`profilesDir` consistent.
3. **Update consumers/GUI** to read from POSIX SHM (`/lfc.telemetry`). Keep JSON parsing unchanged.
4. Optional: enable vendor SDKs for richer GPU metrics.

---

## Notes

* If you still see duplicate GPUs, enable `TRACE` logging temporarily and share `pci` and `hwmon` fields from telemetry; the dedup keys on those.
* To force re-inventory at runtime, trigger detection/RPC instead of polling-induced rescans.

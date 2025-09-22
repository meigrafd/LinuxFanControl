# lfcd — Exit Codes

This document lists the exit status codes returned by the LinuxFanControl daemon (`lfcd`) and what they mean.

> **Scope**: Extracted from `src/daemon/src/main.cpp`.

## Summary

| Code | Meaning                                                                                                                                                                                     |
| ---: | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
|    0 | Successful execution. This includes: normal shutdown after the main loop, or successful `--update`/`--check-update` flow where information (and optionally an asset) is printed/downloaded. |
|    1 | Error during startup/update. Returned when either (a) fetching the latest release info fails during update checks, or (b) daemonization fails.                                              |
|    2 | Configuration/initialization error. Returned when (a) `--update` was requested but `--update-target` is missing, or (b) the daemon fails to initialize (`daemon.init()` returns false).     |
|    3 | Update flow error: the selected release has no downloadable assets.                                                                                                                         |
|    4 | Update flow error: downloading the release asset failed.                                                                                                                                    |

## Details

### 0 — Success

* Normal shutdown path after the main loop completes and resources are cleaned up.
* Also used to indicate a successful `--check-update`/`--update` run (with optional download) where no daemon start is attempted.

### 1 — Startup/Update Failure

* Returned if the update checker cannot fetch the latest release metadata.
* Also returned when daemonization fails (e.g., backgrounding / PID/log setup problems).

### 2 — Init/Argument Error

* In update mode, returned when required `--update-target PATH` is missing.
* Returned when daemon initialization fails (`daemon.init()` returns false).

### 3 — Update: No Assets

* In update mode, the latest release does not provide assets to download.

### 4 — Update: Download Failed

* In update mode, downloading the release asset failed.

---

*This list reflects current return sites in `main.cpp`. If additional exit codes are introduced elsewhere (e.g., new CLI branches), extend this table accordingly.*

## RPC Error Codes (JSON-RPC)

> These codes appear in RPC responses returned by handlers under `src/daemon/src/rpc/*`.
>
> The typical error payload looks like:
>
> ```json
> {
>   "method": "<rpc.method>",
>   "success": false,
>   "error": { "code": -32602, "message": "missing 'jobId'" }
> }
> ```

### Standard JSON‑RPC Codes (used when applicable)

|   Code | Name             | Meaning                                       |
| -----: | ---------------- | --------------------------------------------- |
| -32700 | Parse error      | Invalid JSON was received by the server.      |
| -32600 | Invalid Request  | The JSON sent is not a valid Request object.  |
| -32601 | Method not found | The method does not exist / is not available. |
| -32602 | Invalid params   | Missing or wrong-typed parameter(s).          |
| -32603 | Internal error   | Internal JSON‑RPC error.                      |

### Project‑specific Server Codes (domain errors)

|   Code | Where / Example        | Meaning                                                           |
| -----: | ---------------------- | ----------------------------------------------------------------- |
| -32002 | `profile.save`         | Persistierung fehlgeschlagen (z. B. IO/Serialisierung)            |
| -32004 | `profile.delete`       | Profil nicht gefunden / Dateisystem-Fehler                        |
| -32010 | `config.save`          | Speichern der Konfiguration fehlgeschlagen                        |
| -32031 | `profile.importCancel` | Job not found (e.g., unknown `jobId`).                            |
| -32032 | `profile.importCancel` | Job exists but is not cancelable (already finished/failed).       |
| -32033 | `profile.importCommit` | Commit failed (save/apply step failed; message contains details). |
| -32040 | `list.hwmon`           | HWMON-Inventar nicht verfügbar                                    |
| -32050 | `telemetry.json`       | Telemetrie nicht verfügbar                                        |
| -32060 | `daemon.update`        | Update-Fetch fehlgeschlagen (Netzwerk/HTTP/Parse)                 |
| -32061 | `daemon.update`        | Kein Asset in der neuesten Release                                |
| -32062 | `daemon.update`        | Download fehlgeschlagen                                           |


> **Note:** Handlers may include additional contextual messages in `error.message` (e.g., validation failures, mapping errors). Keep the negative sign for error codes in JSON‑RPC responses.

---

If new handlers introduce additional domain‑specific errors, extend the table above and keep codes within the `-32000..-32099` range to avoid clashing with JSON‑RPC core codes.

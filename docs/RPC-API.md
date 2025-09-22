# RPC API — LinuxFanControl (lfcd)

## Deutsch

Dieses Dokument beschreibt die öffentlichen RPC-Endpunkte des Daemons.

Format: JSON-RPC-ähnliche Requests/Responses. Erfolgsantworten enthalten `{ "success": true, "data": ... }`, Fehler `{ "success": false, "error": { "code": <int>, "message": <string> } }`.

---

## Methodenübersicht

| Methode                | Beschreibung                                         | Parameter (JSON)                                                                                              |                                  |                           |
| ---------------------- | ---------------------------------------------------- | ------------------------------------------------------------------------------------------------------------- | -------------------------------- | ------------------------- |
| `commands`             | Liste aller verfügbaren RPC‑Befehle                  | —                                                                                                             |                                  |                           |
| `help`                 | Hilfe für einen Befehl anzeigen                      | `{ "cmd": "<string>" }` *oder* `"<string>"`                                                                   |                                  |                           |
| `ping`                 | Liveness‑Probe                                       | —                                                                                                             |                                  |                           |
| `version`              | Version von Daemon/RPC zurückgeben                   | —                                                                                                             |                                  |                           |
| `config.get`           | Effektive Daemon‑Konfiguration abrufen               | —                                                                                                             |                                  |                           |
| `config.set`           | Einzelnen Konfigurationswert setzen                  | `{ "key": "<string>", "value": <any> }`                                                                       |                                  |                           |
| `config.save`          | Konfiguration speichern                              | `{ "path": "<string>" }` *(optional)*                                                                         |                                  |                           |
| `daemon.restart`       | Neustart des Daemons anfordern                       | —                                                                                                             |                                  |                           |
| `daemon.shutdown`      | Daemon sauber herunterfahren                         | —                                                                                                             |                                  |                           |
| `daemon.update`        | Nach neuem Release suchen/herunterladen              | —                                                                                                             |                                  |                           |
| `detect.start`         | Nicht‑blockierende Erkennung starten                 | —                                                                                                             |                                  |                           |
| `detect.status`        | Status/Fortschritt der Erkennung                     | —                                                                                                             |                                  |                           |
| `detect.abort`         | Erkennung abbrechen                                  | —                                                                                                             |                                  |                           |
| `detect.results`       | Peak‑RPMs pro PWM zurückgeben                        | —                                                                                                             |                                  |                           |
| `engine.enable`        | Automatische Steuerung aktivieren                    | —                                                                                                             |                                  |                           |
| `engine.disable`       | Automatische Steuerung deaktivieren                  | —                                                                                                             |                                  |                           |
| `engine.reset`         | Steuerung deaktivieren und Profil löschen            | —                                                                                                             |                                  |                           |
| `engine.status`        | Basisstatus der Engine                               | —                                                                                                             |                                  |                           |
| `list.fan`             | Tachoeingänge (RPM) auflisten                        | —                                                                                                             |                                  |                           |
| `list.pwm`             | PWM‑Kontrollen auflisten                             | —                                                                                                             |                                  |                           |
| `list.sensor`          | Temperatureingänge auflisten                         | —                                                                                                             |                                  |                           |
| `profile.list`         | Profile auflisten                                    | —                                                                                                             |                                  |                           |
| `profile.load`         | Profil nach Namen laden                              | `{ "name": "<string>" }`                                                                                      |                                  |                           |
| `profile.save`         | Profil speichern                                     | `{ "name": "<string>" }`                                                                                      |                                  |                           |
| `profile.rename`       | Profil umbenennen                                    | `{ "from": "<string>", "to": "<string>" }`                                                                    |                                  |                           |
| `profile.delete`       | Profil löschen                                       | `{ "name": "<string>" }`                                                                                      |                                  |                           |
| `profile.getActive`    | Namen des aktiven Profils abrufen                    | —                                                                                                             |                                  |                           |
| `profile.setActive`    | Aktives Profil setzen                                | `{ "name": "<string>" }`                                                                                      |                                  |                           |
| `profile.importAs`     | Asynchronen Import starten; gibt `{jobId}` zurück    | `{ "path": "<string>", "name": "<string>", "validateDetect": <bool>, "rpmMin": <int>, "timeoutMs": <int> }` |                                  |                           |
| `profile.importStatus` | Asynchronen Importstatus abrufen                     | `{ "jobId": "<string>" }`                                                                                     | Asynchronen Importstatus abrufen | `{ "jobId": "<string>" }` |
| `profile.importJobs`   | Alle aktiven Importjobs auflisten                    | —                                                                                                             |                                  |                           |
| `profile.importCommit` | Importjob committen: Profil speichern & aktiv setzen | `{ "jobId": "<string>" }`                                                                                     |                                  |                           |
| `profile.importCancel` | Importjob abbrechen                                  | `{ "jobId": "<string>" }`                                                                                     |                                  |                           |
| `telemetry.json`       | Aktuellen SHM‑JSON‑Blob zurückgeben                  | —                                                                                                             |                                  |                           |

## Fehlercodes

### Standard-JSON-RPC-Codes

* `-32700` Parse-Fehler: Ungültiges JSON.
* `-32600` Ungültiges Request-Objekt.
* `-32601` Methode nicht gefunden.
* `-32602` Ungültige Parameter (fehlen oder falscher Typ).
* `-32603` Interner Fehler.

### Projektspezifische Fehlercodes

* `-32031` Importjob nicht gefunden.
* `-32032` Importjob nicht abbrechbar.
* `-32033` Import-Commit fehlgeschlagen.

---

## Beispiel

### Request

```json
{"jsonrpc":"2.0","id":2,"method":"profile.importStatus","params":{"jobId":"42"}}
```

### Erfolgsantwort

```json
{
  "success": true,
  "data": {
    "jobId": "42",
    "state": "running",
    "progress": 85,
    "profileName": "Gaming"
  }
}
```

### Fehlerantwort

```json
{
  "success": false,
  "error": { "code": -32602, "message": "missing jobId" }
}
```

---

*Dieses Dokument spiegelt die aktuell implementierte RPC-API wider. Alle Namen und Codes stammen direkt aus dem Quellcode.*

---

## English

This document describes the public RPC endpoints of the daemon.

Format: JSON-RPC-like requests/responses. Successful replies contain `{ "success": true, "data": ... }`, errors `{ "success": false, "error": { "code": <int>, "message": <string> } }`.

---

## Method Overview

| Method                 | Description                                               | Params (JSON)                                                                                                |
| ---------------------- | --------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------ |
| `commands`             | List available RPC commands                               | —                                                                                                            |
| `help`                 | Show help for a command                                   | `{ "cmd": "<string>" }` *or* `"<string>"`                                                                    |
| `ping`                 | Liveness probe                                            | —                                                                                                            |
| `version`              | Return daemon/rpc version info                            | —                                                                                                            |
| `config.get`           | Get effective daemon configuration                        | —                                                                                                            |
| `config.set`           | Set a single config key                                   | `{ "key": "<string>", "value": <any> }`                                                                      |
| `config.save`          | Save daemon configuration                                 | `{ "path": "<string>" }` *(optional)*                                                                        |
| `daemon.restart`       | Request daemon restart                                    | —                                                                                                            |
| `daemon.shutdown`      | Shutdown daemon gracefully                                | —                                                                                                            |
| `daemon.update`        | Check/download latest release                             | —                                                                                                            |
| `detect.start`         | Start non‑blocking detection                              | —                                                                                                            |
| `detect.status`        | Detection status/progress                                 | —                                                                                                            |
| `detect.abort`         | Abort detection                                           | —                                                                                                            |
| `detect.results`       | Return detection peak RPMs per PWM                        | —                                                                                                            |
| `engine.enable`        | Enable automatic control                                  | —                                                                                                            |
| `engine.disable`       | Disable automatic control                                 | —                                                                                                            |
| `engine.reset`         | Disable control and clear profile                         | —                                                                                                            |
| `engine.status`        | Basic engine status                                       | —                                                                                                            |
| `list.fan`             | List tach inputs (RPM)                                    | —                                                                                                            |
| `list.pwm`             | List PWM controls                                         | —                                                                                                            |
| `list.sensor`          | List temperature inputs                                   | —                                                                                                            |
| `profile.list`         | List profiles                                             | —                                                                                                            |
| `profile.load`         | Load a profile by name                                    | `{ "name": "<string>" }`                                                                                     |
| `profile.save`         | Save a profile                                            | `{ "name": "<string>" }`                                                                                     |
| `profile.rename`       | Rename a profile                                          | `{ "from": "<string>", "to": "<string>" }`                                                                   |
| `profile.delete`       | Delete a profile                                          | `{ "name": "<string>" }`                                                                                     |
| `profile.getActive`    | Get active profile name                                   | —                                                                                                            |
| `profile.setActive`    | Set active profile                                        | `{ "name": "<string>" }`                                                                                     |
| `profile.importAs`     | Start async import; returns `{jobId}`                     | `{ "path": "<string>", "name": "<string>", "validateDetect: <bool>, "rpmMin": <int>, "timeoutMs": <int> }` |
| `profile.importStatus` | Get async import status                                   | `{ "jobId": "<string>" }`                                                                                    |
| `profile.importJobs`   | List all active import jobs                               | —                                                                                                            |
| `profile.importCommit` | Commit a finished import job: save profile and set active | `{ "jobId": "<string>" }`                                                                                    |
| `profile.importCancel` | Cancel an import job                                      | `{ "jobId": "<string>" }`                                                                                    |
| `telemetry.json`       | Return current SHM JSON blob                              | —                                                                                                            |

## Error Codes

### Standard JSON-RPC codes

* `-32700` Parse error: Invalid JSON.
* `-32600` Invalid request object.
* `-32601` Method not found.
* `-32602` Invalid params (missing or wrong type).
* `-32603` Internal error.

### Project-specific domain errors

* `-32031` Import job not found.
* `-32032` Import job not cancelable.
* `-32033` Import commit failed.

---

## Example

### Request

```json
{ "method": "profile.importStatus", "params": { "jobId": "42" } }
```

### Success response

```json
{
  "success": true,
  "data": {
    "jobId": "42",
    "state": "running",
    "progress": 85,
    "profileName": "MyGPU"
  }
}
```

### Error response

```json
{
  "success": false,
  "error": { "code": -32602, "message": "missing jobId" }
}
```

---

*This document reflects the currently implemented RPC API. All names and codes are taken directly from the codebase.*

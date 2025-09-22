# Import Workflow

## Deutsch

### Unterstützte Quellformate

1. **LinuxFanControl Profile (.json)** — Erkennung via `schema` (z.B. `LinuxFanControl.Profile/v1`).
2. **FanControl (Rem0o) – Release-Profile (.json)** — Heuristik über Felder wie `Computers`, `Controls`, `Curves`, `Version`. Siehe dazu auch → [Import FanControl](import_fancontrol.md).

### Ablauf (asynchron)

1. **Start** via `profile.importAs`

   ```json
   {
     "path": "/path/to/source.json",
     "name": "Gaming",
     "validateDetect": true,
     "rpmMin": 600,
     "timeoutMs": 4000
   }
   ```

   **Antwort:** `{ "jobId": "<string>" }`
2. **Status** via `profile.importStatus` — Snapshot der Felder (siehe unten).
3. **Abbruch** via `profile.importCancel`.
4. **Commit** via `profile.importCommit` — Profil speichern & aktiv setzen.
5. **Jobs** via `profile.importJobs` — Liste aktiver Jobs.

### Statusobjekt (ImportStatus)

| Feld                  | Typ    | Bedeutung                                   |
| --------------------- | ------ | ------------------------------------------- |
| `jobId`               | string | Eindeutige Job-ID                           |
| `state`               | string | `pending` \| `running` \| `done` \| `error` |
| `progress`            | number | 0..100 (heuristisch)                        |
| `message`             | string | Menschlich lesbarer Status                  |
| `error`               | string | Fehlermeldung bei Fehlern                   |
| `profileName`         | string | Name des erzeugten Profils                  |
| `isFanControlRelease` | bool   | Quelle war FanControl-Release               |

### Validierung (optional)

* `validateDetect` (bool) aktiviert Prüfschritte.
* `rpmMin` (int > 0) Mindest‑RPM.
* `timeoutMs` (int ≥ 0) Zeitbudget pro PWM.

**Ablauf:** PWM auf 100 %, Tachos gleichen Chips poll’en, Zustand zurücksetzen. Fehler → `state="error"` + Text in `error`.

### Commit‑Semantik

`profile.importCommit` lädt das erzeugte Profil aus dem Job, persistiert es und setzt es als aktiv. Fehler werden als RPC‑Codes zurückgegeben (siehe RPC‑API.md).

---

## English

### Supported source formats

1. **LinuxFanControl Profile (.json)** — detected via `schema` (e.g., `LinuxFanControl.Profile/v1`).
2. **FanControl (Rem0o) — release profiles (.json)** — heuristics over `Computers`, `Controls`, `Curves`, `Version`.

### Flow (asynchronous)

1. **Start** via `profile.importAs`

   ```json
   {
     "path": "/path/to/source.json",
     "name": "Gaming",
     "validateDetect": true,
     "rpmMin": 600,
     "timeoutMs": 4000
   }
   ```

   **Return:** `{ "jobId": "<string>" }`
2. **Status** via `profile.importStatus` — snapshot of fields (see below).
3. **Cancel** via `profile.importCancel`.
4. **Commit** via `profile.importCommit` — save profile & set active.
5. **Jobs** via `profile.importJobs` — list active jobs.

### Status object (ImportStatus)

| Field                 | Type   | Meaning                                     |
| --------------------- | ------ | ------------------------------------------- |
| `jobId`               | string | unique job id                               |
| `state`               | string | `pending` \| `running` \| `done` \| `error` |
| `progress`            | number | 0..100 (heuristic)                          |
| `message`             | string | human‑readable status                       |
| `error`               | string | error message when failing                  |
| `profileName`         | string | name of created profile                     |
| `isFanControlRelease` | bool   | source was FanControl release               |

### Validation (optional)

* `validateDetect` (bool) enables checks.
* `rpmMin` (int > 0) minimum RPM.
* `timeoutMs` (int ≥ 0) per‑PWM budget.

**Procedure:** set PWM to 100%, poll tachs on same chip, restore state. Failures → `state="error"` with message.

### Commit semantics

`profile.importCommit` loads the profile from the job, persists it, and sets it active. Failures return RPC error codes (see RPC‑API.md).

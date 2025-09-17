````markdown
# Linux Fan Control — `lfcd` JSON‑RPC API

This document lists all RPC commands exposed by the daemon and their possible parameters.
Requests use the [JSON‑RPC 2.0](https://www.jsonrpc.org/specification) envelope over TCP (default `127.0.0.1:8777`).

> **Envelope**
>
> ```json
> {"jsonrpc":"2.0","id":1,"method":"<method>","params":{...}}
> ```
>
> **Response**
>
> ```json
> {"jsonrpc":"2.0","id":"1","result":{"method":"<method>","success":true,"data":{...}}}
> ```
>
> On error: `{"success":false,"error":{"code":<int>,"message":"..."}}`.

---

## Command index (quick reference)

| Method | Summary | Params (optional •) |
|---|---|---|
| `ping` | Health check | — |
| `version` | RPC & daemon version | — |
| `rpc.commands` | List available RPC methods | — |
| `config.load` | Load daemon config from disk | — |
| `config.save` | Save daemon config | `{ config: object }` |
| `config.set` | Set a single config key | `{ key: string, value: any }` |
| `hwmon.snapshot` | Counts of discovered devices | — |
| `list.sensor` | List temperature sensors | — |
| `list.fan` | List tach inputs (RPM) | — |
| `list.pwm` | List PWM outputs | — |
| `list.profiles` | List stored profiles (names) | — |
| `engine.enable` | Enable automatic control | — |
| `engine.disable` | Disable automatic control | — |
| `engine.reset` | Disable control & clear profile | — |
| `engine.status` | Engine state & timing | — |
| `detect.start` | Start non‑blocking detection | — |
| `detect.abort` | Abort detection | — |
| `detect.status` | Detection progress | — |
| `detect.results` | Detection RPMs per PWM | — |
| `profile.getActive` | Get active profile name | — |
| `profile.verifyMapping` | Validate mapping (and optionally run detection) | `{ name•: string, path•: string, withDetect•: bool, rpmMin•: int, requireAllPwms•: bool }` |
| `profile.import` | Import a FanControl.Releases JSON and apply (no save) | `{ path: string }` |
| `profile.importAs` | Import FanControl.Releases JSON, validate, save & apply | `{ path: string, name: string, validateDetect•: bool, rpmMin•: int, requireAllPwms•: bool }` |
| `profile.load` | Load & apply stored profile | `{ name: string }` |
| `profile.set` | Write raw profile JSON to disk | `{ name: string, profile: object }` |
| `profile.delete` | Delete stored profile | `{ name: string }` |
| `telemetry.json` | Current shared‑memory telemetry blob | — |
| `daemon.update` | Check/download latest release | `{ download•: bool, target•: string, repo•: "owner/repo" }` |
| `daemon.restart` | Request daemon restart | — |
| `daemon.shutdown` | Graceful shutdown | — |

---

## Command details

### ping
**Params:** —  
**Returns:** `{}`

### version
**Params:** —  
**Returns:** `{ daemon: "lfcd", version: "<semver>", rpc: 1 }`

### rpc.commands
**Params:** —  
**Returns:** `[{ name: string, help: string }, ...]`

### config.load
**Params:** —  
**Returns:**
```json
{
  "log": {"file": string, "debug": bool, "level": string},
  "rpc": {"host": string, "port": int},
  "shm": {"path": string},
  "profiles": {"dir": string, "active": string},
  "pidFile": string,
  "engine": {"deltaC": number, "forceTickMs": int, "tickMs": int}
}
````

### config.save

**Params:** `{ config: object }`
Accepted fields mirror `config.load` output. Unknown keys are ignored.

### config.set

**Params:** `{ key: string, value: any }`
Supported keys:

* `log.debug` (bool)
* `log.file` (string)
* `log.level` (string)
* `rpc.host` (string)
* `rpc.port` (int)
* `shm.path` (string)
* `profiles.dir` (string)
* `profiles.active` (string)
* `pidFile` (string)
* `engine.deltaC` (number 0.0..10.0)
* `engine.forceTickMs` (int 100..10000)
* `engine.tickMs` (int 5..1000)

### hwmon.snapshot

**Params:** —
**Returns:** `{ temps: int, fans: int, pwms: int }`

### list.sensor

**Params:** —
**Returns:** `[{ index:int, path:string, label:string, tempC:number }, ...]`

### list.fan

**Params:** —
**Returns:** `[{ index:int, path:string, rpm:int }, ...]`

### list.pwm

**Params:** —
**Returns:** `[{ index:int, pwm:string, enable:int, percent:int, raw:int, enablePath:string }, ...]`

### list.profiles

**Params:** —
**Returns:** `["Name1", "Name2", ...]` (no `.json` extension)

### engine.enable / engine.disable / engine.reset / engine.status

**Params:** —
`engine.status` **Returns:** `{ enabled: bool, deltaC:number, tickMs:int, forceTickMs:int, activeProfile:string }`

### detect.start / detect.abort / detect.status / detect.results

**Params:** —

* `detect.start` **Returns:** `{ started: bool }`
* `detect.status` **Returns:** `{ running: bool, currentIndex: int, total: int, phase: string }`
* `detect.results` **Returns:** `[{ index:int, rpm:int }, ...]`

### profile.getActive

**Params:** —
**Returns:** `{ name: string }`

### profile.verifyMapping

Validate an existing native profile file (`name`) or a JSON file on disk (`path`).

**Params:**

```ts
{
  name?: string,          // profile name (no .json required)
  path?: string,          // absolute path to a profile JSON
  withDetect?: boolean,   // default: false — run live detection before verdict
  rpmMin?: number,        // default: 300  — minimal RPM for pass
  requireAllPwms?: boolean // default: true — strict: all mapped PWMs must pass
}
```

**Returns:**

```json
{
  "ok": bool,
  "errors": string[],
  "warnings": string[],
  "pwms": [{"pwmPath": string, "exists": bool}],
  "temps": [{"tempPath": string, "exists": bool}],
  "detect": {
    "requested": bool,
    "ran": bool,
    "rpmMin": int,
    "perPwm": [{"pwmIndex": int, "rpm": int, "ok": bool, "reason"?: string}],
    "results"?: int[]
  }
}
```

### profile.import

Import a **FanControl.Releases** config and apply it **without saving**.

**Params:** `{ path: string }`

### profile.importAs

Import **FanControl.Releases**, validate, **save** under `name` and **apply**.
If validation fails, nothing is saved or applied.

**Params:**

```ts
{
  path: string,           // source FanControl.Releases JSON
  name: string,           // target native profile name (without .json)
  validateDetect?: boolean, // default: true
  rpmMin?: number,        // default: 300
  requireAllPwms?: boolean // default: true (set false to allow partial pass)
}
```

**Returns:**

```json
{
  "name": string,
  "applied": boolean,
  "savedPath"?: string,
  "warnings": string[],
  "verify": <same as profile.verifyMapping>
}
```

### profile.load

**Params:** `{ name: string }`
**Returns:** `{}`

### profile.set

**Params:** `{ name: string, profile: object }`
Writes raw JSON to `<profilesDir>/<name>.json`.

### profile.delete

**Params:** `{ name: string }`
Deletes `<profilesDir>/<name>.json`.

### telemetry.json

**Params:** —
**Returns:** raw telemetry JSON string (as produced by the engine/shm).

### daemon.update

Check GitHub Releases and optionally download an asset.

**Params:**

```ts
{
  download?: boolean,      // default: false
  target?: string,         // required if download=true (path to write)
  repo?: string            // default: "meigrafd/LinuxFanControl"
}
```

**Returns:**

```json
{
  "current": string,
  "latest": string,
  "name": string,
  "html": string,
  "updateAvailable": bool,
  "assets": [{"name": string, "url": string, "type": string, "size": number}],
  "downloaded"?: bool,
  "target"?: string
}
```

### daemon.restart / daemon.shutdown

**Params:** —
**Returns:** `{}`

---

## Examples

### Import & save a FanControl profile (strict)

```bash
printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"profile.importAs","params":{"path":"/path/to/userConfig.json","name":"Default"}}' \
| nc 127.0.0.1 8777
```

### Import with partial pass allowed

```bash
printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"profile.importAs","params":{"path":"/path/to/userConfig.json","name":"Default","requireAllPwms":false}}' \
| nc 127.0.0.1 8777
```

### Validate an existing stored profile with live detection

```bash
printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"profile.verifyMapping","params":{"name":"Default","withDetect":true,"rpmMin":300}}' \
| nc 127.0.0.1 8777
```


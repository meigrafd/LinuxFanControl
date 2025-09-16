Modernes, schnelles Fan Control mit GUI im Stil von [FanControl.Release](https://github.com/Rem0o/FanControl.Releases) für Linux:

09.2025: **GUI Funktioniert noch nicht!**

## Funktionsumfang
- Hintergrundprozess mit der Logik von GUI getrennt, damit die Lüftersteuerung auch ohne GUI läuft.
  - Einbindung von libsensors + /sys/class/hwmon um möglichst alle Lüfter und Sensoren zu erkennen.
  - Hybrid Protokoll: JSON-RPC 2.0 für Config/Control. Telemetry läuft über POSIX Shared Memory (SHM) Ringbuffer - deutlich performanter, kein RPC-Polling.
- Automatische Erkennung und Kalibrierung der verfügbaren Sensoren und Lüfter.
- Steuerung über Mix, Trigger oder Graph.
- FanControl.Release Config importierbar.

### Daemon
- `--config PATH` default `~/.config/LinuxFanControl/daemon.json`
- `--pidfile PATH` default `/run/lfcd.pid` fallback `/tmp/lfcd.pid`
- `--logfile PATH` default `/var/log/lfc/daemon.log` fallback `/tmp/daemon_lfc.log`
- `--profiles DIR` default `~/.config/LinuxFanControl/profiles/`
- `--profile NAME` default `Default`
- `--host` for RPC server, default `127.0.0.1`
- `--port` default `8777`
- `--shm PATH` default `/dev/shm/lfc_telemetry`
- `--foreground`
- `--debug`
- `--cmds`
- `--check-update`
- `--update`
- `--update-target PATH`
- `--download-update --target <file> [--repo <owner/name>]`

## Install
### Installiere je nach Distribution:

Fedora:
```bash
sudo dnf install \
  gcc-c++ cmake make pkgconfig git \
  nlohmann-json-devel lm_sensors lm_sensors-devel libcurl-devel \
  dotnet-sdk-9.0 \
  libX11 libXrandr libXcursor libXrender mesa-libgbm \
  fontconfig freetype libicu
```
Ubuntu/Debian:
```bash
sudo apt install \
  build-essential cmake pkg-config git \
  nlohmann-json3-dev libsensors-dev lm-sensors libcurl4-openssl-dev \
  dotnet-sdk-9.0 \
  libx11-6 libxrandr2 libxrender1 libxcursor1 libgbm1 \
  libfontconfig1 libfreetype6 libicu*
```
Arch:
```bash
sudo pacman -S --needed \
  base-devel cmake git \
  nlohmann-json lm_sensors curl \
  dotnet-sdk \
  libx11 libxrandr libxrender libxcursor libgbm \
  fontconfig freetype2 icu
```

## Compile

Build:
```bash
./build.sh
```
Build GUI:
```bash
dotnet publish -c Release -r linux-x64 --self-contained false
# (für Portable-Binary: --self-contained true)
```

Debug Run:
```bash
./run.sh gui

# Oder manuell:
export LFC_DAEMON=../../../build/src/lfcd
export LFC_DAEMON_ARGS="--debug"
dotnet run -c Debug
```

## Quickstart:
- GUI starten → Setup-Button → Auto-Detection & Kalibrierung.
- Danach erscheinen oben die PWM-Tiles (verschiebbar), unten Triggers/Curves/Mixes.
- Importer: FanControl.Release-JSON einlesen (Basis-Mapping).
- Profile verwalten und Kurven/Channel-Zuweisungen konfigurieren.

## Rechte & PWM-Schreibzugriff
Für viele /sys/class/hwmon/.../pwm*-Knoten ist root oder eine udev-Regel erforderlich:
```bash
# /etc/udev/rules.d/99-lfc.rules
KERNEL=="pwm*", MODE="0660", GROUP="lfc"
KERNEL=="fan*_*", MODE="0440", GROUP="lfc"
```
Danach:
```bash
sudo groupadd -f lfc
sudo usermod -aG lfc $USER
sudo udevadm control --reload-rules && sudo udevadm trigger
newgrp lfc
```

## Konfiguration
- Pfad: `~/.config/LinuxFanControl/daemon.json` (wird automatisch angelegt).
- Versionierung & Backups: Beim Speichern wird eine Kopie `config_YYYYMMDD_HHMMSS.bak.json` erzeugt.
- Sprachdateien (erweiterbar): `Locales/en.json`, `Locales/de.json`
- Themes (erweiterbar): `Themes/default.json`

##  CPU-Load Optimization:
Diese besonderen ENV Einstellungen sind auch in der Konfigurationsdatei `daemon.json` enthalten und können auch zur Laufzeit über `config.set engine.deltaC / engine.forceTickMs` geändert werden.
- LFC_TICK_MS
  - Standard: 25
  - Bereich: 5..1000 (ms)
  - Takt des Mainloops (Engine tick()-Frequenz). Höher = weniger CPU-Last, träger; niedriger = reaktiver.
- LFC_DELTA_C
  - Standard: 0.5 (°C)
  - Bereich: 0.0..10.0
  - Temperatur-Schwellwert für Regel-Ticks: engine.tick() läuft nur, wenn mind. ein Sensor sich um ≥ Delta geändert hat (Detection ausgenommen). Senkt Leerlauf-CPU-Last.
- LFC_FORCE_TICK_MS
  - Standard: 1000
  - Bereich: 100..10000 (ms)
  - Sicherheits-Intervall: Spätestens alle forceTickMs wird ein Tick erzwungen, auch ohne Temperaturänderung.

## Debug
Test JSON-RPC
```bash
curl -s -X POST http://127.0.0.1:8777/rpc \
     -H 'Content-Type: application/json' \
     -d '{"jsonrpc":"2.0","id":1,"method":"rpc.commands"}'
```
Batch
```bash
curl -s -X POST http://127.0.0.1:8777/rpc \
     -H 'Content-Type: application/json' \
     -d '[{"jsonrpc":"2.0","id":"a","method":"rpc.commands"},
          {"jsonrpc":"2.0","id":"b","method":"engine.start","params":{}},
          {"jsonrpc":"2.0","method":"engine.stop"}]' | jq
```
Enumerate
```bash
curl -s http://127.0.0.1:8777/rpc \
     -H 'content-type: application/json' \
     -d '{"jsonrpc":"2.0","id":2,"method":"telemetry.json"}' | jq
```
JSON-RPC Test per netcat (eine Zeile pro Request):
```bash
printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"version"}' | nc 127.0.0.1 8777
printf '%s\n' '{"jsonrpc":"2.0","id":2,"method":"telemetry.json"}'   | nc 127.0.0.1 8777
printf '%s\n' '[{"jsonrpc":"2.0","id":"a","method":"rpc.commands"},{"jsonrpc":"2.0","method":"engine.start"}]' | nc 127.0.0.1 8777
printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"config.set","params":{"key":"engine.deltaC","value":0.7}}' | nc 127.0.0.1 8777
printf '%s\n' '{"jsonrpc":"2.0","id":2,"method":"config.set","params":{"key":"engine.forceTickMs","value":1500}}' | nc 127.0.0.1 8777
```
SHM lesen (einfacher Dumper):
```bash
python3 - <<'PY'
import mmap, posix_ipc, struct, sys, time
name="/lfc_telemetry"
slotSize=1024
capacity=512
shm=posix_ipc.SharedMemory(name)
m=mmap.mmap(shm.fd, struct.calcsize("8sIIIII")+slotSize*capacity)
shm.close_fd()
hdr=m.read(8+4*5)
magic,ver,cap,ss,wi,_=struct.unpack("8sIIIII",hdr)
print("magic",magic,"ver",ver,"cap",cap,"ss",ss,"wi",wi)
m.seek(8+4*5)
for i in range(10):
    print(m.read(ss).split(b'\0',1)[0].decode(errors='ignore').strip())
PY
```

Modernes, schnelles Fan Control mit GUI im Stil von **FanControl.Release** für Linux:

## Funktionsumfang
- Hintergrundprozess mit der Logik von GUI getrennt, damit die Lüftersteuerung auch ohne GUI läuft.
  - Einbindung von libsensors + /sys/class/hwmon um möglichst alle Lüfter und Sensoren zu erkennen.
  - Hybrid Protokoll: JSON-RPC 2.0 für Config/Control. Telemetry läuft über POSIX Shared Memory (SHM) Ringbuffer - deutlich performanter, kein RPC-Polling.
- Multilanguage support über i18n json Dateien.
- Theme support.
- Automatische Erkennung und Kalibrierung der verfügbaren Sensoren und Lüfter.
- Nicht relevante Sensoren können abgewählt und ausgeblendet werden.
- Steuerung über Mix, Trigger oder Graph.
- FanControl.Release Config importierbar.
 

## Install
### Installiere je nach Distribution:

Fedora:
```bash
sudo dnf install \
  gcc-c++ cmake make pkgconfig git \
  nlohmann-json-devel lm_sensors lm_sensors-devel \
  dotnet-sdk-9.0 \
  libX11 libXrandr libXcursor libXrender mesa-libgbm \
  fontconfig freetype libicu
```
Ubuntu/Debian:
```bash
sudo apt install \
  build-essential cmake pkg-config git \
  nlohmann-json3-dev libsensors-dev lm-sensors \
  dotnet-sdk-9.0 \
  libx11-6 libxrandr2 libxrender1 libxcursor1 libgbm1 \
  libfontconfig1 libfreetype6 libicu*
```
Arch:
```bash
sudo pacman -S --needed \
  base-devel cmake git \
  nlohmann-json lm_sensors \
  dotnet-sdk \
  libx11 libxrandr libxrender libxcursor libgbm \
  fontconfig freetype2 icu
```

Build:
```bash
FRESH=1 ./build.sh
```
Release-Build GUI:
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
- Theme (Dark/Light) und Sprache (en/de) sind live umschaltbar.
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
- Pfad: `~/.config/LinuxFanControl/config.json` (wird automatisch angelegt).
- Versionierung & Backups: Beim Speichern wird eine Kopie `config_YYYYMMDD_HHMMSS.bak.json` erzeugt.
- Sprachdateien: `i18n/en/`, `i18n/de/` (erweiterbar).
- Themes: `Themes/Dark.xaml`, `Themes/Light.xaml` (anpassbar/erweiterbar).

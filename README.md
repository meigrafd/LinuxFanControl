Clone vom Windows tool "Fan Control" für Linux, da mir CoolerControl usw zu unübersichtlich sind.

## Funktionsumfang
- Optik angelehnt ans Original, weil mir das sehr gut gefällt und dadurch auch ein Umstieg Win->Linux leichter ist.
- Hintergrundprozess mit der Logik von GUI getrennt, damit die Lüftersteuerung auch ohne GUI läuft.
  - Einbindung von libsensors + /sys/class/hwmon um möglichst alle Lüfter und Sensoren zu erkennen.
  - Hybrid Protokoll: JSON-RPC 2.0 für Config/Control. Telemetry läuft über POSIX Shared Memory (SHM) Ringbuffer - deutlich performanter, kein RPC-Polling.
- Theme im GUI umschaltbar.
- Multilanguage support über i18n json Dateien.
- Automatische Erkennung und Kalibrierung der verfügbaren Sensoren und Lüfter.
- Nicht relevante Sensoren können abgewählt und ausgeblendet werden.
- Steuerung über Mix, Trigger oder Graph.
- FanControl.Release Config importierbar.
 

## Install
```bash
sudo dnf install gcc-c++ cmake make qt6-qtbase-devel qt6-qttools-devel lm_sensors lm_sensors-devel nlohmann-json-devel

mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
cmake --build . -j

./lfcd &         # Daemon starten
./lfc-gui        # GUI starten
```

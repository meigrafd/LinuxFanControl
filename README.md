Clone vom Windows tool "Fan Control" für Linux, da mir CoolerControl usw zu unübersichtlich sind.

## Funktionsumfang
- Optik angelehnt ans Original, weil mir das sehr gut gefällt und dadurch auch ein Umstieg Win->Linux leichter ist.
- Hintergrundprozess mit der Logik von GUI getrennt, damit die Lüftersteuerung auch ohne GUI läuft.
  - Einbindung von libsensors + /sys/class/hwmon um möglichst alle Lüfter und Sensoren zu erkennen.
  - Einfache JSON-RPC 2.0 (newline-delimited JSON). Parser ist leichtgewichtig (kein Fremd-JSON-Header).
  - Implementierte RPCs: `enumerate`, `listChannels`, `createChannel`, `deleteChannel`, `setChannelMode`, `setChannelManual`, `setChannelCurve`, `setChannelHystTau`, `engineStart`, `engineStop`, `deleteCoupling` (Stub mit Erfolgsmeldung).
- UNIX-Socket-RPC zum Daemon.
- Light/Dark-Theme im GUI umschaltbar.
- Multilanguage support über i18n json Dateien, umschaltbar in der GUI.
- Automatische Erkennung und Kalibrierung der verfügbaren Sensoren und Lüfter.
- Nicht relevante Sensoren können abgewählt und ausgeblendet werden.
- Steuerung über Mix, Trigger oder Graph.
 

## Install
```bash
sudo dnf install gcc-c++ cmake make qt6-qtbase-devel qt6-qttools-devel lm_sensors lm_sensors-devel nlohmann-json-devel

mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
cmake --build . -j

./lfcd &         # Daemon starten
./lfc-gui        # GUI starten
```

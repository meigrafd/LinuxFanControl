Clone vom Windows tool "Fan Control" für Linux, da mir CoolerControl usw zu unübersichtlich sind.

## Funktionsumfang
- Opitk angelehnt ans Original, weil mir das sehr gut gefällt und dadurch auch ein Umstieg Win->Linux leichter ist.
- Hintergrundprozess mit der Logik und GUI getrennt, damit die Lüftersteuerung auch ohne GUI läuft.
  - Einbindung von libsensors um möglichst alle Lüfter und Sensoren zu erkennen, sowie robustere Steuerung.
- UNIX-Socket-RPC zum Daemon.
- Light/Dark-Theme im GUI umschaltbar.
- Multilanguage support über i18n json Dateien, umschaltbar in der GUI.
- Automatische Erkennung und Kalibrierung der verfügbaren Sensoren und Lüfter.
- Nicht relevante Sensoren können abgewählt und ausgeblendet werden.
- Steuerung über Mix, Trigger oder Graph Editor (drag, add, delete) mit Hysteresis.
 

## Install
```bash
sudo apt install lm_sensors lm_sensors-devel cmake gcc-c++ socat
```

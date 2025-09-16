## Import von FanControl.Releases-Profilen (Windows → Linux)

Das Projekt unterstützt den Import von Konfigurationen aus [Rem0o/FanControl.Releases](https://github.com/Rem0o/FanControl.Releases).<br>
Da FanControl für **Windows** entwickelt ist, müssen die dort definierten Sensoren und Lüfterausgänge auf Linux-Systeme gemappt werden.

### Mapping-Strategie

**Temperatursensoren**

* Zuweisung erfolgt anhand von `hwmon`-Einträgen unter `/sys/class/hwmon`.
* Primär wird nach passenden **Labels** gesucht (`temp*_label`, z.B. `Tctl`, `CPU Die`, `edge`, `junction`, `nvme_temp`).
* Fallback: Zuordnung nach **Treiberquelle** (`k10temp`, `coretemp`, `amdgpu`, `nvidia`, `nvme`, `acpitz`) und Index-Heuristiken.
* Nur Werte im plausiblen Bereich (-20 °C bis 150 °C) werden akzeptiert.
* Kann ein Sensor nicht eindeutig zugeordnet werden, bricht der Import mit einer Fehlermeldung ab.

**PWM-Ausgänge und Lüfter**

* Jeder in FanControl definierte Control-Kanal wird einem `hwmon`-PWM zugeordnet.
* Bevorzugt über die Topologie: PWMs und Tachos innerhalb desselben `hwmonX`, z.B. `pwm1` ↔ `fan1_input`.
* Die Zuordnung wird später durch die **Detection-Routine** validiert: PWM kurz ansteuern und prüfen, ob ein Tachometer reagiert.
* Bei fehlender oder unsicherer Zuordnung gibt der Import eine Warnung zurück.

**Kurven und Regeln**

* Temperatur-zu-PWM-Kurven werden 1:1 übernommen.
* Alle Punkte und Hysteresen bleiben erhalten.
* Einheiten: °C bleiben °C, PWM-Werte werden in Prozent (0–100 %) abgebildet.

### Sicherheitsmechanismen

* **Harte Fehler** bei nicht auflösbaren Sensoren oder Controls → kein stillschweigendes „Raten“.
* **Warnungen** bei unsicheren Matches (z.B. generische `temp1_input`).
* Nach einem Import kann optional die Detection gestartet werden, um PWM↔Fan-Paare praktisch zu bestätigen.
* Importiert & lädt das Profil in die Engine. Die Engine bleibt aber zunächst **deaktiviert**. (Safety: es wird nichts automatisch geregelt).

### Praktische Nutzung

1. Import mit

   ```bash
   printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"profile.importAs","params":{"path":"/PATH/TO/userConfig.json","name":"Default"}}' | nc 127.0.0.1 8777 | jq
   ```
2. Anschließend mit

   ```bash
   printf '%s\n' '[{"jsonrpc":"2.0","method":"list.sensor"},{"jsonrpc":"2.0","method":"list.fan"},{"jsonrpc":"2.0","method":"list.pwm"}]' | nc 127.0.0.1 8777 | jq
   ```

   prüfen, ob die Pfade/Labels sinnvoll zugeordnet sind.
3. `engine.enable` und `telemetry.json` verwenden, um die Wirkung live zu beobachten.
4. Optional `detect.start` und danach `detect.results` aufrufen, um die maximalen RPM je PWM zu sehen.

### Alternativ mit strenger Validierung

Wichtig: Die Validierung prüft u.a. ob alle in den Regeln referenzierten pwmPath/tempPaths existieren, ob Kurven konsistent sind, und – falls gewünscht – führt sie eine synchrone Detection durch und vergleicht die gemessenen Drehzahlen mit den im Profil verwendeten PWM-Kanälen (Default-Schwelle 300 RPM).

   ```bash
   printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"profile.verifyMapping","params":{"path":"/PATH/TO/userConfig.json","withDetect":true}}' | nc 127.0.0.1 8777 | jq
   ```

### Manuelle Anpassungen

Falls der automatische Import nicht perfekt passt:

* Profil zunächst mit `profile.importAs` unter einem Namen speichern.
* JSON-Datei im `profilesDir` öffnen und Sensoren oder PWMs manuell anpassen.
* Mit `profile.load` erneut aktivieren.

# ESP32 MQTT RGB Bus Bridge + ATmega328P RGB Slave

Dieses Repo enthält zwei Arduino-Sketches:

- `esp32/esp32_mqtt_bridge.ino`: ESP32-Wroom-32 als MQTT-Bridge, I2C-Master und WLAN-Konfigurationsportal.
- `atmega328p/atmega_rgb_slave.ino`: ATmega328P als I2C-Slave für eine 5mm RGB-LED mit dynamischer Adressvergabe.

## Architektur

1. Handy-App veröffentlicht MQTT-Nachrichten auf Topics `rgbled/1`, `rgbled/2`, usw.
2. ESP32 empfängt die Nachricht und sendet RGB-Werte über I2C an den passenden ATmega328P.
3. Jeder ATmega steuert genau eine RGB-LED per PWM.
4. Ein neu angeschlossener ATmega bekommt automatisch eine I2C-Adresse zugeteilt.

## WLAN-Konfiguration

- Beim ersten Start ohne gespeicherte Zugangsdaten startet der ESP32 ein WLAN `ESP32-RGB-Setup`.
- Verbinde dich mit diesem WLAN und rufe `http://192.168.4.1` auf.
- Dort kannst du SSID und Passwort speichern; der ESP32 startet danach neu.
- Im WLAN-Modus meldet sich der ESP32 per mDNS als `esp32-rgb-bridge` und bietet einen HTTP-Service an.
- Die Weboberfläche erlaubt Farbsteuerung, Effekte und die Auswahl der ATmega-Targets.
- Die Liste der Targets wird automatisch aktualisiert, sobald ein neuer ATmega eine Adresse erhalten hat.

## MQTT Payload

Format: `R,G,B` (z. B. `255,80,0`). Werte sind 0–255.

## I2C Bus

- ESP32 ist der Master.
- Jeder Atmega startet mit der Adresse `0x08` und erhält vom ESP32 eine neue Adresse (ab `0x10`).
- Der ESP32 merkt sich die nächste freie Adresse im Flash.
- SDA/SCL gemeinsam verbinden, plus Pull-Ups (z. B. 4.7k) nach 3.3V.

## Anpassungen

### ESP32

In `esp32/esp32_mqtt_bridge.ino`:

- `kMqttHost` / `kMqttPort` auf euren Broker setzen.
- `kI2cFirstDynamicAddress` / `kI2cLastDynamicAddress` anpassen.

### ATmega328P

In `atmega328p/atmega_rgb_slave.ino`:

- PWM-Pins anpassen, falls nötig.
- `kInvertPwm = true` wenn ihr eine common-anode LED verwendet.

## Verdrahtung (Kurzfassung)

- ESP32 SDA/SCL an ATmega SDA/SCL (mit Pegelanpassung 3.3V/5V beachten).
- GND gemeinsam.
- RGB-LED über Vorwiderstände an PWM-Pins des ATmega.

## Beispiel MQTT Topics

- `rgbled/1` → ATmega mit Adresse 0x10
- `rgbled/2` → ATmega mit Adresse 0x11
- `rgbled/3` → ATmega mit Adresse 0x12
- `rgbled/4` → ATmega mit Adresse 0x13

## Android App (Android 13+)

Die App liegt unter `android-app/` und nutzt MQTT, um RGB-Daten an den Broker zu senden.

- Beim ersten Start wird per mDNS nach `esp32-rgb-bridge` gesucht.
- Gefundene ESP32-Geräte werden gespeichert und beim nächsten Start automatisch genutzt.
- Weitere ESP32 lassen sich im seitlichen Menü (☰ oben links) hinzufügen.
- Im Menü werden alle gespeicherten ESP32 angezeigt und können umbenannt werden.
- Ein Farbkreis sowie manuelle RGB-Eingabe senden Änderungen direkt als `R,G,B` an das MQTT-Topic.
- Ziel-ATmegas können ausgewählt werden (Standard: alle).
- Effekte wie Flackern und Rainbow lassen sich aktivieren und werden an den ESP32 übertragen.
- Die App fragt regelmäßig den Status ab und aktualisiert die Target-Liste automatisch.

## Web UI (ESP32)

Im WLAN-Modus kann die Weboberfläche über `http://<esp32-ip>/` genutzt werden:

- Farbkreis und RGB-Felder senden direkt an die ausgewählten ATmega-Targets.
- Effekte (Flackern, Rainbow) können gestartet/gestoppt werden.
- Standardmäßig sind alle Targets aktiviert.
- Die Weboberfläche aktualisiert die Target-Liste automatisch, sobald neue ATmegas verbunden werden.

## Home Assistant Integration (MQTT)

Der ESP32 veröffentlicht MQTT Discovery für Home Assistant auf Basis der vergebenen I2C-Adressen.

### Schritt-für-Schritt

1. **MQTT Broker einrichten**
   - In Home Assistant: *Einstellungen → Add-ons* (oder externer Broker).
   - Mosquitto Add-on installieren/starten, Benutzer/Passwort anlegen.
2. **MQTT Integration hinzufügen**
   - *Einstellungen → Geräte & Dienste → Integration hinzufügen → MQTT*.
   - Broker-Adresse, Port, Benutzer/Passwort eintragen.
3. **ESP32 auf den Broker konfigurieren**
   - In `esp32/esp32_mqtt_bridge.ino` `kMqttHost`, `kMqttPort`, `kMqttUser`, `kMqttPassword` setzen.
   - Sketch flashen und neu starten.
4. **Discovery prüfen**
   - Nach erfolgreicher MQTT-Verbindung veröffentlicht der ESP32 Discovery-Daten.
   - In Home Assistant erscheinen neue Lichter `ESP32 RGB 1`, `ESP32 RGB 2`, usw.
5. **Steuerung testen**
   - Ein Light auswählen und Farbe setzen.
   - Der ESP32 empfängt `rgbled/<index>` mit `R,G,B` Payload.

**Hinweis:** Die Entitäten arbeiten im optimistischen Modus und nutzen das Topic `rgbled/<index>`.

## Serial Monitor Debug

Der ESP32 gibt beim Start wichtige Infos aus (WLAN/IP, AP-IP, MQTT-Status, neue I2C-Adressen).

1. Baudrate auf **115200** setzen.
2. Nach dem Start erscheinen z. B.:
   - `AP IP: 192.168.4.1` (Provisioning-Modus)
   - `WLAN verbunden, IP: <esp32-ip>`
   - `Webserver im WLAN-Modus gestartet.`
   - `MQTT verbunden.`
   - `I2C Scan gestartet...` / `I2C Gerät gefunden: 0x..`

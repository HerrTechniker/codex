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

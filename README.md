# ESP32 MQTT RGB Bus Bridge + ATmega328P RGB Slave

Dieses Repo enthält zwei Arduino-Sketches:

- `esp32/esp32_mqtt_bridge.ino`: ESP32-Wroom-32 als MQTT-Bridge und I2C-Master.
- `atmega328p/atmega_rgb_slave.ino`: ATmega328P als I2C-Slave für eine 5mm RGB-LED.

## Architektur

1. Handy-App veröffentlicht MQTT-Nachrichten auf Topics `rgbled/1`, `rgbled/2`, usw.
2. ESP32 empfängt die Nachricht und sendet RGB-Werte über I2C an den passenden ATmega328P.
3. Jeder ATmega steuert genau eine RGB-LED per PWM.

## MQTT Payload

Format: `R,G,B` (z. B. `255,80,0`). Werte sind 0–255.

## I2C Bus

- ESP32 ist der Master.
- Jeder ATmega bekommt eine eigene Adresse (z. B. 0x10, 0x11, 0x12, 0x13).
- SDA/SCL gemeinsam verbinden, plus Pull-Ups (z. B. 4.7k) nach 3.3V.

## Anpassungen

### ESP32

In `esp32/esp32_mqtt_bridge.ino`:

- `kWifiSsid` / `kWifiPassword` eintragen.
- `kMqttHost` / `kMqttPort` auf euren Broker setzen.
- `kI2cAddresses` auf Anzahl der ATmegas anpassen.

### ATmega328P

In `atmega328p/atmega_rgb_slave.ino`:

- `kSlaveAddress` für jeden ATmega anpassen.
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

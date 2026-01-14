#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>

// WiFi credentials
const char *kWifiSsid = "YOUR_WIFI_SSID";
const char *kWifiPassword = "YOUR_WIFI_PASSWORD";

// MQTT broker settings
const char *kMqttHost = "broker.hivemq.com";
const uint16_t kMqttPort = 1883;
const char *kMqttClientId = "esp32-rgb-bridge";
const char *kMqttUser = "";     // Leave empty if not required
const char *kMqttPassword = ""; // Leave empty if not required

// MQTT topics
const char *kTopicBase = "rgbled"; // Example: rgbled/1

// I2C settings
const uint8_t kI2cAddresses[] = {0x10, 0x11, 0x12, 0x13};
const size_t kI2cAddressCount = sizeof(kI2cAddresses) / sizeof(kI2cAddresses[0]);

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);

String buildTopic(uint8_t index) {
  return String(kTopicBase) + "/" + String(index + 1);
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(kWifiSsid, kWifiPassword);

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
  }
}

void connectMqtt() {
  mqtt_client.setServer(kMqttHost, kMqttPort);

  while (!mqtt_client.connected()) {
    if (mqtt_client.connect(kMqttClientId, kMqttUser, kMqttPassword)) {
      for (size_t i = 0; i < kI2cAddressCount; ++i) {
        mqtt_client.subscribe(buildTopic(i).c_str());
      }
    } else {
      delay(1000);
    }
  }
}

bool parseRgbPayload(const char *payload, uint8_t &r, uint8_t &g, uint8_t &b) {
  int values[3] = {0, 0, 0};
  int value_index = 0;
  int current = 0;
  for (size_t i = 0; payload[i] != '\0'; ++i) {
    char c = payload[i];
    if (c >= '0' && c <= '9') {
      current = current * 10 + (c - '0');
      if (current > 255) {
        current = 255;
      }
    } else if (c == ',' && value_index < 3) {
      values[value_index++] = current;
      current = 0;
    }
  }
  if (value_index < 3) {
    values[value_index++] = current;
  }
  if (value_index != 3) {
    return false;
  }
  r = static_cast<uint8_t>(values[0]);
  g = static_cast<uint8_t>(values[1]);
  b = static_cast<uint8_t>(values[2]);
  return true;
}

void sendRgbToSlave(uint8_t address, uint8_t r, uint8_t g, uint8_t b) {
  Wire.beginTransmission(address);
  Wire.write(r);
  Wire.write(g);
  Wire.write(b);
  Wire.endTransmission();
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  char payload_buffer[32];
  if (length >= sizeof(payload_buffer)) {
    return;
  }
  memcpy(payload_buffer, payload, length);
  payload_buffer[length] = '\0';

  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  if (!parseRgbPayload(payload_buffer, r, g, b)) {
    return;
  }

  for (size_t i = 0; i < kI2cAddressCount; ++i) {
    String expected = buildTopic(i);
    if (expected.equals(topic)) {
      sendRgbToSlave(kI2cAddresses[i], r, g, b);
      return;
    }
  }
}

void setup() {
  Wire.begin();
  connectWifi();
  mqtt_client.setCallback(mqttCallback);
  connectMqtt();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWifi();
  }
  if (!mqtt_client.connected()) {
    connectMqtt();
  }
  mqtt_client.loop();
}

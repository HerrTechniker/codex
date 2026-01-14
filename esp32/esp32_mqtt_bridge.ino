#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <Wire.h>

// MQTT broker settings
const char *kMqttHost = "broker.hivemq.com";
const uint16_t kMqttPort = 1883;
const char *kMqttClientId = "esp32-rgb-bridge";
const char *kMqttUser = "";     // Leave empty if not required
const char *kMqttPassword = ""; // Leave empty if not required

// MQTT topics
const char *kTopicBase = "rgbled"; // Example: rgbled/1

// I2C addressing
const uint8_t kI2cDefaultSlaveAddress = 0x08;
const uint8_t kI2cFirstDynamicAddress = 0x10;
const uint8_t kI2cLastDynamicAddress = 0x30;
const uint8_t kAssignCommand = 0xA0;

// WiFi AP settings for provisioning
const char *kProvisionApSsid = "ESP32-RGB-Setup";

Preferences preferences;
WebServer server(80);
WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);

String wifi_ssid;
String wifi_password;
uint8_t next_i2c_address = kI2cFirstDynamicAddress;

String buildTopic(uint8_t index) {
  return String(kTopicBase) + "/" + String(index + 1);
}

void loadStoredSettings() {
  preferences.begin("rgb-bridge", false);
  wifi_ssid = preferences.getString("ssid", "");
  wifi_password = preferences.getString("pass", "");
  next_i2c_address = preferences.getUChar("next_i2c", kI2cFirstDynamicAddress);
}

void saveWifiSettings(const String &ssid, const String &pass) {
  preferences.putString("ssid", ssid);
  preferences.putString("pass", pass);
  wifi_ssid = ssid;
  wifi_password = pass;
}

void saveNextI2cAddress(uint8_t address) {
  preferences.putUChar("next_i2c", address);
  next_i2c_address = address;
}

String buildProvisionPage(const String &message) {
  String page = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
  page += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  page += "<title>ESP32 WLAN Setup</title></head><body>";
  page += "<h2>ESP32 WLAN Setup</h2>";
  if (message.length() > 0) {
    page += "<p>" + message + "</p>";
  }
  page += "<form method='POST' action='/save'>";
  page += "<label>SSID</label><br><input name='ssid' required><br>";
  page += "<label>Passwort</label><br><input name='pass' type='password'><br><br>";
  page += "<button type='submit'>Speichern</button>";
  page += "</form></body></html>";
  return page;
}

void handleRoot() {
  server.send(200, "text/html", buildProvisionPage(""));
}

void handleSave() {
  if (!server.hasArg("ssid")) {
    server.send(400, "text/plain", "SSID fehlt");
    return;
  }
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  saveWifiSettings(ssid, pass);
  server.send(200, "text/html", buildProvisionPage("Gespeichert. ESP32 startet neu..."));
  delay(1000);
  ESP.restart();
}

void startProvisioningPortal() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(kProvisionApSsid);
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}

bool connectWifi() {
  if (wifi_ssid.isEmpty()) {
    return false;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
  }
  return WiFi.status() == WL_CONNECTED;
}

void connectMqtt() {
  mqtt_client.setServer(kMqttHost, kMqttPort);
  while (!mqtt_client.connected()) {
    if (mqtt_client.connect(kMqttClientId, kMqttUser, kMqttPassword)) {
      for (uint8_t i = 0; i < (kI2cLastDynamicAddress - kI2cFirstDynamicAddress + 1); ++i) {
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

  int topic_index = -1;
  for (uint8_t i = 0; i < (kI2cLastDynamicAddress - kI2cFirstDynamicAddress + 1); ++i) {
    if (buildTopic(i).equals(topic)) {
      topic_index = i;
      break;
    }
  }
  if (topic_index < 0) {
    return;
  }

  uint8_t address = kI2cFirstDynamicAddress + static_cast<uint8_t>(topic_index);
  sendRgbToSlave(address, r, g, b);
}

bool slavePresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void assignAddressIfNeeded() {
  if (next_i2c_address > kI2cLastDynamicAddress) {
    return;
  }
  if (!slavePresent(kI2cDefaultSlaveAddress)) {
    return;
  }
  Wire.beginTransmission(kI2cDefaultSlaveAddress);
  Wire.write(kAssignCommand);
  Wire.write(next_i2c_address);
  if (Wire.endTransmission() == 0) {
    saveNextI2cAddress(next_i2c_address + 1);
  }
}

void setup() {
  Wire.begin();
  loadStoredSettings();

  mqtt_client.setCallback(mqttCallback);

  if (!connectWifi()) {
    startProvisioningPortal();
    return;
  }
  connectMqtt();
}

void loop() {
  if (WiFi.getMode() == WIFI_AP) {
    server.handleClient();
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (!connectWifi()) {
      startProvisioningPortal();
      return;
    }
  }
  if (!mqtt_client.connected()) {
    connectMqtt();
  }
  mqtt_client.loop();
  assignAddressIfNeeded();
}

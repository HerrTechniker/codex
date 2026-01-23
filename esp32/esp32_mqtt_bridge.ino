#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <Wire.h>
#include <ESPmDNS.h>
#include <math.h>

// MQTT broker settings
const char *kMqttHost = "broker.hivemq.com";
const uint16_t kMqttPort = 1883;
const char *kMqttClientId = "esp32-rgb-bridge";
const char *kMqttUser = "";     // Leave empty if not required
const char *kMqttPassword = ""; // Leave empty if not required

// MQTT topics
const char *kTopicBase = "rgbled"; // Example: rgbled/1
const char *kEffectTopic = "rgbled/effect";

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
uint8_t effect_targets[kI2cLastDynamicAddress - kI2cFirstDynamicAddress + 1] = {0};
uint8_t last_i2c_scan_count = 0;

void handleRoot();
void handleSave();
void handleControl();
void handleEffect();
void handleStatus();
void logI2cScan();
uint8_t scanI2cDevices(bool log_results);

enum EffectMode {
  kEffectNone = 0,
  kEffectFlicker,
  kEffectRainbow,
};

void logLine(const String &message) {
  Serial.println(message);
}

EffectMode current_effect = kEffectNone;
unsigned long last_effect_update = 0;
float rainbow_hue = 0.0f;

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

String buildControlPage() {
  String page = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
  page += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  page += "<title>ESP32 RGB Control</title></head><body>";
  page += "<h2>ESP32 RGB Control</h2>";
  page += "<form method='POST' action='/control' id='color-form'>";
  page += "<label>Farbe</label><br><input type='color' id='color' value='#ffffff'><br>";
  page += "<label>R</label><input name='r' id='r' type='number' min='0' max='255' value='255'>";
  page += "<label>G</label><input name='g' id='g' type='number' min='0' max='255' value='255'>";
  page += "<label>B</label><input name='b' id='b' type='number' min='0' max='255' value='255'><br><br>";
  page += "<strong>ATmega Auswahl</strong><br><div id='targets-color'></div>";
  page += "<br><button type='submit'>Senden</button>";
  page += "</form><hr>";
  page += "<form method='POST' action='/effect' id='effect-form'>";
  page += "<label>Effekt</label><br>";
  page += "<select name='effect'>";
  page += "<option value='none'>Kein Effekt</option>";
  page += "<option value='flicker'>Flackern</option>";
  page += "<option value='rainbow'>Rainbow</option>";
  page += "</select><br><br>";
  page += "<strong>ATmega Auswahl</strong><br><div id='targets-effect'></div>";
  page += "<br><button type='submit'>Effekt starten</button>";
  page += "</form>";
  page += "<script>";
  page += "const renderTargets=(count)=>{";
  page += "const color=document.getElementById('targets-color');";
  page += "const effect=document.getElementById('targets-effect');";
  page += "const build=(container)=>{container.innerHTML='';";
  page += "for(let i=1;i<=count;i++){";
  page += "const label=document.createElement('label');";
  page += "const cb=document.createElement('input');";
  page += "cb.type='checkbox';cb.name='t';cb.value=i;cb.checked=true;";
  page += "label.appendChild(cb);label.append(' LED '+i);";
  page += "container.appendChild(label);container.appendChild(document.createElement('br'));}};";
  page += "build(color);build(effect);};";
  page += "const updateTargets=()=>{fetch('/status').then(r=>r.json()).then(data=>{";
  page += "const count=data.count||0;renderTargets(count>0?count:1);}).catch(()=>{});};";
  page += "const color=document.getElementById('color');";
  page += "const r=document.getElementById('r');";
  page += "const g=document.getElementById('g');";
  page += "const b=document.getElementById('b');";
  page += "color.addEventListener('input',()=>{";
  page += "const hex=color.value.substring(1);";
  page += "r.value=parseInt(hex.substring(0,2),16);";
  page += "g.value=parseInt(hex.substring(2,4),16);";
  page += "b.value=parseInt(hex.substring(4,6),16);";
  page += "});";
  page += "updateTargets();setInterval(updateTargets,5000);";
  page += "</script></body></html>";
  return page;
}

void handleRoot() {
  if (WiFi.getMode() == WIFI_AP) {
    server.send(200, "text/html", buildProvisionPage(""));
  } else {
    server.send(200, "text/html", buildControlPage());
  }
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
  logLine(String("AP gestartet: ") + kProvisionApSsid);
  logLine(String("AP IP: ") + WiFi.softAPIP().toString());
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/control", HTTP_POST, handleControl);
  server.on("/effect", HTTP_POST, handleEffect);
  server.on("/status", HTTP_GET, handleStatus);
  server.begin();
  logLine("Webserver im AP-Modus gestartet.");
}

bool connectWifi() {
  if (wifi_ssid.isEmpty()) {
    logLine("Keine gespeicherten WLAN-Daten gefunden.");
    return false;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  logLine(String("Verbinde mit WLAN: ") + wifi_ssid);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
  }
  if (WiFi.status() != WL_CONNECTED) {
    logLine("WLAN-Verbindung fehlgeschlagen.");
    return false;
  }
  logLine(String("WLAN verbunden, IP: ") + WiFi.localIP().toString());
  if (!MDNS.begin("esp32-rgb-bridge")) {
    logLine("mDNS Start fehlgeschlagen.");
    return true;
  }
  MDNS.addService("http", "tcp", 80);
  logLine("mDNS gestartet: esp32-rgb-bridge.local");
  return true;
}

void connectMqtt() {
  mqtt_client.setServer(kMqttHost, kMqttPort);
  while (!mqtt_client.connected()) {
    if (mqtt_client.connect(kMqttClientId, kMqttUser, kMqttPassword)) {
      for (uint8_t i = 0; i < (kI2cLastDynamicAddress - kI2cFirstDynamicAddress + 1); ++i) {
        mqtt_client.subscribe(buildTopic(i).c_str());
      }
      mqtt_client.subscribe(kEffectTopic);
      publishStatus();
      publishHaDiscovery();
      logLine("MQTT verbunden.");
    } else {
      logLine("MQTT Verbindung fehlgeschlagen, retry...");
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

uint8_t assignedCount() {
  if (next_i2c_address <= kI2cFirstDynamicAddress) {
    return 0;
  }
  return static_cast<uint8_t>(next_i2c_address - kI2cFirstDynamicAddress);
}

uint8_t scanI2cDevices(bool log_results) {
  uint8_t found = 0;
  for (uint8_t address = kI2cFirstDynamicAddress; address <= kI2cLastDynamicAddress; ++address) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      ++found;
      if (log_results) {
        logLine(String("I2C Gerät gefunden: 0x") + String(address, HEX));
      }
    }
  }
  last_i2c_scan_count = found;
  return found;
}

void publishStatus() {
  if (!mqtt_client.connected()) {
    return;
  }
  uint8_t count = scanI2cDevices(false);
  if (count < assignedCount()) {
    count = assignedCount();
  }
  String payload = String("{\"count\":") + String(count) + "}";
  mqtt_client.publish("rgbled/status", payload.c_str(), true);
}

void publishHaDiscovery() {
  if (!mqtt_client.connected()) {
    return;
  }
  uint8_t count = assignedCount();
  for (uint8_t i = 1; i <= count; ++i) {
    String topic = "homeassistant/light/esp32_rgb_" + String(i) + "/config";
    String payload = "{";
    payload += "\"name\":\"ESP32 RGB " + String(i) + "\",";
    payload += "\"unique_id\":\"esp32_rgb_" + String(i) + "\",";
    payload += "\"command_topic\":\"" + String(kTopicBase) + "/" + String(i) + "\",";
    payload += "\"rgb\":true,";
    payload += "\"rgb_command_template\":\"{{ red }},{{ green }},{{ blue }}\",";
    payload += "\"optimistic\":true";
    payload += "}";
    mqtt_client.publish(topic.c_str(), payload.c_str(), true);
  }
}

void handleStatus() {
  uint8_t count = scanI2cDevices(false);
  if (count < assignedCount()) {
    count = assignedCount();
  }
  String payload = String("{\"count\":") + String(count) + "}";
  server.send(200, "application/json", payload);
}

void clearEffectTargets() {
  for (uint8_t i = 0; i < (kI2cLastDynamicAddress - kI2cFirstDynamicAddress + 1); ++i) {
    effect_targets[i] = 0;
  }
}

void setTargetsFromArgs() {
  clearEffectTargets();
  bool found = false;
  int args = server.args();
  for (int i = 0; i < args; ++i) {
    if (server.argName(i) == "t") {
      int index = server.arg(i).toInt();
      if (index > 0) {
        uint8_t offset = static_cast<uint8_t>(index - 1);
        if (offset < (kI2cLastDynamicAddress - kI2cFirstDynamicAddress + 1)) {
          effect_targets[offset] = 1;
          found = true;
        }
      }
    }
  }
  if (!found) {
    for (uint8_t i = 0; i < (kI2cLastDynamicAddress - kI2cFirstDynamicAddress + 1); ++i) {
      effect_targets[i] = 1;
    }
    return;
  }
}

void sendRgbToTargets(uint8_t r, uint8_t g, uint8_t b, const uint8_t *targets) {
  for (uint8_t i = 0; i < (kI2cLastDynamicAddress - kI2cFirstDynamicAddress + 1); ++i) {
    if (targets == nullptr || targets[i] == 1) {
      uint8_t address = kI2cFirstDynamicAddress + i;
      sendRgbToSlave(address, r, g, b);
    }
  }
}

void handleControl() {
  uint8_t r = static_cast<uint8_t>(server.arg("r").toInt());
  uint8_t g = static_cast<uint8_t>(server.arg("g").toInt());
  uint8_t b = static_cast<uint8_t>(server.arg("b").toInt());
  setTargetsFromArgs();
  current_effect = kEffectNone;
  sendRgbToTargets(r, g, b, effect_targets);
  server.send(200, "text/plain", "OK");
}

void handleEffect() {
  String effect = server.arg("effect");
  setTargetsFromArgs();
  if (effect == "flicker") {
    current_effect = kEffectFlicker;
  } else if (effect == "rainbow") {
    current_effect = kEffectRainbow;
  } else {
    current_effect = kEffectNone;
  }
  server.send(200, "text/plain", "OK");
}

bool parseEffectPayload(const char *payload, EffectMode &mode, uint8_t *targets) {
  String message(payload);
  int effect_pos = message.indexOf("effect=");
  if (effect_pos >= 0) {
    int effect_end = message.indexOf(';', effect_pos);
    String value = message.substring(effect_pos + 7, effect_end < 0 ? message.length() : effect_end);
    if (value == "flicker") {
      mode = kEffectFlicker;
    } else if (value == "rainbow") {
      mode = kEffectRainbow;
    } else {
      mode = kEffectNone;
    }
  } else {
    if (message.startsWith("flicker")) {
      mode = kEffectFlicker;
    } else if (message.startsWith("rainbow")) {
      mode = kEffectRainbow;
    } else {
      mode = kEffectNone;
    }
  }

  int targets_pos = message.indexOf("targets=");
  for (uint8_t i = 0; i < (kI2cLastDynamicAddress - kI2cFirstDynamicAddress + 1); ++i) {
    targets[i] = 0;
  }
  if (targets_pos < 0) {
    for (uint8_t i = 0; i < (kI2cLastDynamicAddress - kI2cFirstDynamicAddress + 1); ++i) {
      targets[i] = 1;
    }
    return true;
  }
  int start = targets_pos + 8;
  while (start < message.length()) {
    int comma = message.indexOf(',', start);
    if (comma < 0) {
      comma = message.length();
    }
    int index = message.substring(start, comma).toInt();
    if (index > 0) {
      uint8_t offset = static_cast<uint8_t>(index - 1);
      if (offset < (kI2cLastDynamicAddress - kI2cFirstDynamicAddress + 1)) {
        targets[offset] = 1;
      }
    }
    start = comma + 1;
  }
  return true;
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  char payload_buffer[32];
  if (length >= sizeof(payload_buffer)) {
    return;
  }
  memcpy(payload_buffer, payload, length);
  payload_buffer[length] = '\0';

  if (strcmp(topic, kEffectTopic) == 0) {
    EffectMode mode = kEffectNone;
    uint8_t targets[kI2cLastDynamicAddress - kI2cFirstDynamicAddress + 1] = {0};
    if (parseEffectPayload(payload_buffer, mode, targets)) {
      for (uint8_t i = 0; i < (kI2cLastDynamicAddress - kI2cFirstDynamicAddress + 1); ++i) {
        effect_targets[i] = targets[i];
      }
      current_effect = mode;
    }
    return;
  }

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
    logLine("Kein ATmega unter Default-Adresse 0x08 gefunden.");
    return;
  }
  Wire.beginTransmission(kI2cDefaultSlaveAddress);
  Wire.write(kAssignCommand);
  Wire.write(next_i2c_address);
  if (Wire.endTransmission() == 0) {
    saveNextI2cAddress(next_i2c_address + 1);
    publishStatus();
    publishHaDiscovery();
    logLine(String("Neue I2C Adresse vergeben: 0x") + String(next_i2c_address - 1, HEX));
  } else {
    logLine("I2C-Adressvergabe fehlgeschlagen (NACK).");
  }
}

void logI2cScan() {
  logLine("I2C Scan gestartet...");
  uint8_t found = scanI2cDevices(true);
  if (found == 0) {
    logLine("Keine I2C Geräte gefunden.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  logLine("ESP32 RGB Bridge startet...");
  Wire.begin();
  loadStoredSettings();
  clearEffectTargets();
  for (uint8_t i = 0; i < (kI2cLastDynamicAddress - kI2cFirstDynamicAddress + 1); ++i) {
    effect_targets[i] = 1;
  }

  mqtt_client.setCallback(mqttCallback);

  if (!connectWifi()) {
    startProvisioningPortal();
    return;
  }
  logI2cScan();
  server.on("/", handleRoot);
  server.on("/control", HTTP_POST, handleControl);
  server.on("/effect", HTTP_POST, handleEffect);
  server.on("/status", HTTP_GET, handleStatus);
  server.begin();
  logLine("Webserver im WLAN-Modus gestartet.");
  connectMqtt();
  publishStatus();
  publishHaDiscovery();
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

  if (current_effect == kEffectFlicker) {
    if (millis() - last_effect_update > 80) {
      last_effect_update = millis();
      uint8_t brightness = static_cast<uint8_t>(180 + (esp_random() % 76));
      uint8_t r = brightness;
      uint8_t g = static_cast<uint8_t>(brightness * 0.55f);
      uint8_t b = static_cast<uint8_t>(brightness * 0.08f);
      sendRgbToTargets(r, g, b, effect_targets);
    }
  } else if (current_effect == kEffectRainbow) {
    if (millis() - last_effect_update > 50) {
      last_effect_update = millis();
      rainbow_hue += 2.0f;
      if (rainbow_hue >= 360.0f) {
        rainbow_hue = 0.0f;
      }
      float hue = rainbow_hue;
      float saturation = 1.0f;
      float value = 1.0f;
      float c = value * saturation;
      float x = c * (1 - fabsf(fmodf(hue / 60.0f, 2) - 1));
      float m = value - c;
      float r1 = 0;
      float g1 = 0;
      float b1 = 0;
      if (hue < 60) {
        r1 = c;
        g1 = x;
      } else if (hue < 120) {
        r1 = x;
        g1 = c;
      } else if (hue < 180) {
        g1 = c;
        b1 = x;
      } else if (hue < 240) {
        g1 = x;
        b1 = c;
      } else if (hue < 300) {
        r1 = x;
        b1 = c;
      } else {
        r1 = c;
        b1 = x;
      }
      uint8_t r = static_cast<uint8_t>((r1 + m) * 255);
      uint8_t g = static_cast<uint8_t>((g1 + m) * 255);
      uint8_t b = static_cast<uint8_t>((b1 + m) * 255);
      sendRgbToTargets(r, g, b, effect_targets);
    }
  }
}

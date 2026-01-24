#include <Wire.h>
#include <EEPROM.h>

// Default address for fresh devices
const uint8_t kDefaultSlaveAddress = 0x08;
const uint8_t kMinValidAddress = 0x08;
const uint8_t kMaxValidAddress = 0x77;
const uint8_t kAssignCommand = 0xA0;
const int kEepromAddress = 0;

// PWM pins for RGB LED
const uint8_t kRedPin = 3;
const uint8_t kGreenPin = 5;
const uint8_t kBluePin = 6;

// Set to true for common-anode LEDs (invert PWM)
const bool kInvertPwm = false;

volatile uint8_t red_value = 0;
volatile uint8_t green_value = 0;
volatile uint8_t blue_value = 0;
uint8_t i2c_address = kDefaultSlaveAddress;

void applyColor(uint8_t r, uint8_t g, uint8_t b) {
  if (kInvertPwm) {
    r = 255 - r;
    g = 255 - g;
    b = 255 - b;
  }
  analogWrite(kRedPin, r);
  analogWrite(kGreenPin, g);
  analogWrite(kBluePin, b);
}

uint8_t loadStoredAddress() {
  uint8_t stored = EEPROM.read(kEepromAddress);
  if (stored >= kMinValidAddress && stored <= kMaxValidAddress) {
    return stored;
  }
  return kDefaultSlaveAddress;
}

void saveAddress(uint8_t address) {
  EEPROM.update(kEepromAddress, address);
}

void applyNewAddress(uint8_t address) {
  i2c_address = address;
  saveAddress(address);
  Wire.begin(i2c_address);
}

void onReceive(int byte_count) {
  if (byte_count <= 0) {
    return;
  }
  uint8_t first = Wire.read();
  byte_count--;
  if (first == kAssignCommand && byte_count >= 1) {
    uint8_t new_address = Wire.read();
    while (Wire.available()) {
      Wire.read();
    }
    if (new_address >= kMinValidAddress && new_address <= kMaxValidAddress) {
      applyNewAddress(new_address);
    }
    return;
  }

  if (byte_count < 2) {
    while (Wire.available()) {
      Wire.read();
    }
    return;
  }

  red_value = first;
  green_value = Wire.read();
  blue_value = Wire.read();
  while (Wire.available()) {
    Wire.read();
  }
}

void setup() {
  pinMode(kRedPin, OUTPUT);
  pinMode(kGreenPin, OUTPUT);
  pinMode(kBluePin, OUTPUT);

  applyColor(0, 0, 0);

  i2c_address = loadStoredAddress();
  Wire.begin(i2c_address);
  Wire.onReceive(onReceive);
}

void loop() {
  applyColor(red_value, green_value, blue_value);
  delay(10);
}

#include <Wire.h>

// Change this address per ATmega328P
const uint8_t kSlaveAddress = 0x10;

// PWM pins for RGB LED
const uint8_t kRedPin = 3;
const uint8_t kGreenPin = 5;
const uint8_t kBluePin = 6;

// Set to true for common-anode LEDs (invert PWM)
const bool kInvertPwm = false;

volatile uint8_t red_value = 0;
volatile uint8_t green_value = 0;
volatile uint8_t blue_value = 0;

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

void onReceive(int byte_count) {
  if (byte_count < 3) {
    while (Wire.available()) {
      Wire.read();
    }
    return;
  }
  red_value = Wire.read();
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

  Wire.begin(kSlaveAddress);
  Wire.onReceive(onReceive);
}

void loop() {
  applyColor(red_value, green_value, blue_value);
  delay(10);
}

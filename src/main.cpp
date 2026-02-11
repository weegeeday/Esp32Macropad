#include <Adafruit_NeoPixel.h>
#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <HomeSpan.h>

#include "ConfigManager.h"
#include "MacroButton.h"
#include "MacropadLight.h"

#include "RotaryEncoder.h"

// Pin Definitions
#ifdef PIN_NEOPIXEL
#undef PIN_NEOPIXEL
#endif
#define MACROPAD_NEOPIXEL 12
// Using specific name to avoid redefining standard PIN_NEOPIXEL if it leaks

// Rotary Encoder
#define PIN_ENC_A 5
#define PIN_ENC_B 10
#define PIN_ENC_SW 4

// Buttons
const int buttonPins[8] = {34, 35, 36, 37, 38, 39, 40, 41};

#define NUM_PIXELS 8

Adafruit_NeoPixel pixels(NUM_PIXELS, MACROPAD_NEOPIXEL, NEO_GRB + NEO_KHZ800);
ConfigManager globalConfig;
Adafruit_USBD_HID usb_hid;
RotaryEncoder encoder(PIN_ENC_A, PIN_ENC_B, PIN_ENC_SW);
MacroButton *buttons[8];

// HID Report Descriptor for a standard keyboard
uint8_t const desc_hid_report[] = {TUD_HID_REPORT_DESC_KEYBOARD()};

void setupNeoPixels() {
  pixels.begin();
  pixels.setBrightness(50);
  pixels.show(); // Initialize all pixels to 'off'
}

void setPixelColor(uint8_t r, uint8_t g, uint8_t b, int pixelIdx = -1) {
  if (pixelIdx < 0) {
    for (int i = 0; i < NUM_PIXELS; i++) {
      // Don't overwrite brightness, use Color()
      pixels.setPixelColor(i, pixels.Color(r, g, b));
    }
  } else if (pixelIdx < NUM_PIXELS) {
    pixels.setPixelColor(pixelIdx, pixels.Color(r, g, b));
  }
  pixels.show();
}

void pinsetup() {
  // NeoPixel handled by library

  // Rotary Encoder
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);

  // Buttons Matrix handled by MacroButton class, but initialized here ensures
  // pullups are set early
  for (int i = 0; i < 8; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
}

void setup() {
  Serial.begin(115200);

  // HID Setup
  usb_hid.setPollInterval(10);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.begin();

  // Config Setup
  globalConfig.begin();

  pinsetup();
  setupNeoPixels();

  // Initialize HomeSpan
  homeSpan.begin(Category::Bridges, "Macropad Bridge");

  // Bridge Accessory
  new SpanAccessory();
  new Service::AccessoryInformation();
  new Characteristic::Identify();

  // Light Accessory (Global Control)
  new MacropadLight();

  // Button Accessories
  new SpanAccessory();
  new Service::AccessoryInformation();
  new Characteristic::Identify();
  new Characteristic::Name("Macropad Buttons");

  for (int i = 0; i < 8; i++) {
    // Buttons are now stored globally to be polled in loop
    buttons[i] = new MacroButton(buttonPins[i], i, i);
  }

  encoder.begin();
}

void loop() {
  homeSpan.poll();
  encoder.poll();

  for (int i = 0; i < 8; i++) {
    if (buttons[i])
      buttons[i]->loop();
  }

  // Check config updates from Serial
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    globalConfig.processSerialCommand(cmd);
  }
}
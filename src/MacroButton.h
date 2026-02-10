#ifndef MACRO_BUTTON_H
#define MACRO_BUTTON_H

#include "ConfigManager.h"
#include <Adafruit_TinyUSB.h>
#include <HomeSpan.h>


extern void setPixelColor(uint8_t r, uint8_t g, uint8_t b, int pixelIdx);
extern Adafruit_USBD_HID usb_hid;

class MacroButton : public Service::StatelessProgrammableSwitch {
  int pin;
  int index;
  int pixelIdx;
  SpanCharacteristic *switchEvent;

  // Debounce
  unsigned long lastPressTime = 0;
  bool isPressed = false;

public:
  MacroButton(int pin, int index, int pixelIdx)
      : Service::StatelessProgrammableSwitch(), pin(pin), index(index),
        pixelIdx(pixelIdx) {
    // Only initialize HomeKit char if mode is HOMEKIT
    // Ideally we should create this Service conditionally in main.cpp
    // But for simplicity/hybrid, we can have the service always present
    // and just not update it if in HID mode?
    // HomeSpan doesn't easily allow dynamic addition/removal of Services
    // without reboot + re-init. So we will instantiate it, but only update
    // *switchEvent* if type is HK.

    switchEvent = new Characteristic::ProgrammableSwitchEvent();
    pinMode(pin, INPUT_PULLUP);
  }

  void loop() {
    bool currentState = digitalRead(pin) == LOW; // Active LOW
    unsigned long now = millis();

    if (currentState != isPressed) {
      // State change
      if (currentState) {
        // PRESSED
        if (now - lastPressTime > 50) { // Debounce 50ms
          onPress();
          isPressed = true;
          lastPressTime = now;
        }
      } else {
        // RELEASED
        if (now - lastPressTime > 50) {
          onRelease();
          isPressed = false;
          lastPressTime = now;
        }
      }
    }
  }

  void onPress() {
    ButtonConfig &cfg = globalConfig.config.buttons[index];

    // Feedback: Turn off LED on press
    setPixelColor(0, 0, 0, pixelIdx);

    if (cfg.type == BUTTON_HOMEKIT) {
      // Single Press Event
      switchEvent->setVal(0);
    } else if (cfg.type == BUTTON_HID) {
      // Generic HID Keyboard press
      // For now, map value to 'a' + value (just for testing uniqueness)
      // Real implementation needs proper usage codes
      uint8_t keycode[6] = {0};
      keycode[0] = (uint8_t)cfg.value;
      usb_hid.keyboardReport(0, 0, keycode);
    }
  }

  void onRelease() {
    ButtonConfig &cfg = globalConfig.config.buttons[index];

    // Feedback: Restore color
    setPixelColor(cfg.color[0], cfg.color[1], cfg.color[2], pixelIdx);

    if (cfg.type == BUTTON_HID) {
      // Release key
      usb_hid.keyboardRelease(0);
    }
  }
};

#endif

#ifndef MACRO_BUTTON_H
#define MACRO_BUTTON_H

#include "ConfigManager.h"
#include <Adafruit_TinyUSB.h>
#include <HomeSpan.h>

extern void setPixelColor(uint8_t r, uint8_t g, uint8_t b, int pixelIdx);
extern Adafruit_USBD_HID usb_hid;

// Macro Opcodes
#define MACRO_OP_END 0x00
#define MACRO_OP_PRESS 0x01
#define MACRO_OP_RELEASE 0x02
#define MACRO_OP_DELAY 0x03

class MacroButton : public Service::StatelessProgrammableSwitch {
  int pin;
  int index;
  int pixelIdx;
  SpanCharacteristic *switchEvent;

  // Debounce
  unsigned long lastPressTime = 0;
  bool isPressed = false;

  // Macro Playback State
  bool isPlayingMacro = false;
  int macroPtr = 0;
  unsigned long waitStart = 0;
  unsigned long waitDuration = 0;

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
    handleMacroPlayback(); // Run macro logic every loop

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
      // HID Keyboard press with modifiers
      uint8_t keycode[6] = {0};
      keycode[0] = (uint8_t)cfg.value;

      // cfg.modifiers is directly compatible with HID modifier bitmap
      // (Ctrl=1, Shift=2, Alt=4, Gui=8, etc)
      usb_hid.keyboardReport(0, cfg.modifiers, keycode);
    } else if (cfg.type == BUTTON_MACRO) {
      startMacro(cfg.value);
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
    // Note: We don't stop macros on release, they usually play to completion
    // unless we want hold-to-repeat or stop-on-release logic?
    // For now, let's assume "fire and forget" or "run until end".
  }

  void startMacro(int offset) {
    if (offset < 0 || offset >= MAX_MACRO_BYTES)
      return;

    macroPtr = offset;
    isPlayingMacro = true;
    waitDuration = 0; // Ready to start immediately
  }

  void handleMacroPlayback() {
    if (!isPlayingMacro)
      return;

    // Check if we are waiting
    if (waitDuration > 0) {
      if (millis() - waitStart < waitDuration) {
        return; // Still waiting
      }
      waitDuration = 0; // Wait finished
    }

    // Process next opcode
    while (isPlayingMacro && waitDuration == 0) {
      if (macroPtr >= MAX_MACRO_BYTES) {
        isPlayingMacro = false;
        return;
      }

      uint8_t opcode = globalConfig.config.macroBuffer[macroPtr++];

      switch (opcode) {
      case MACRO_OP_END:
        isPlayingMacro = false;
        break;

      case MACRO_OP_PRESS: {
        if (macroPtr + 2 > MAX_MACRO_BYTES) {
          isPlayingMacro = false;
          break;
        }
        uint8_t mod = globalConfig.config.macroBuffer[macroPtr++];
        uint8_t key = globalConfig.config.macroBuffer[macroPtr++];

        uint8_t keycode[6] = {0};
        keycode[0] = key;
        usb_hid.keyboardReport(0, mod, keycode);
        break;
      }

      case MACRO_OP_RELEASE: {
        if (macroPtr + 2 > MAX_MACRO_BYTES) {
          isPlayingMacro = false;
          break;
        }
        uint8_t mod = globalConfig.config.macroBuffer[macroPtr++];
        uint8_t key = globalConfig.config.macroBuffer[macroPtr++];

        // For simple releaseAll, we might use keyboardRelease(0)
        // But if we want specific key release, we'd need to track state.
        // HID keyboardReport is "current state".
        // So "Release" usually means "send report without this key".
        // But simplify: If we want to simulate "Press A, Release A",
        // OP_PRESS sends [A], OP_RELEASE sends [] (if it's the only key).

        // Simplified Implementation: Release ALL for now?
        // Or just send empty report?
        // The opcode has mod+key args, meaning we *could* be specific,
        // but TinyUSB HID acts on "Report".

        // Workaround: We will just release ALL for this step if it's a
        // "RELEASE" op logic. Ideally, we should maintain a set of pressed
        // keys. But for basic macros, usually we do: Press A, Wait, Release,
        // Wait...

        usb_hid.keyboardRelease(0);
        break;
      }

      case MACRO_OP_DELAY: {
        if (macroPtr + 2 > MAX_MACRO_BYTES) {
          isPlayingMacro = false;
          break;
        }
        uint8_t timeHigh = globalConfig.config.macroBuffer[macroPtr++];
        uint8_t timeLow = globalConfig.config.macroBuffer[macroPtr++];

        waitDuration = (timeHigh << 8) | timeLow;
        waitStart = millis();
        break;
      }

      default:
        // Unknown opcode, stop
        isPlayingMacro = false;
        break;
      }
    }
  }
};

#endif

#ifndef MACRO_BUTTON_H
#define MACRO_BUTTON_H

#include "ConfigManager.h"
#include <HomeSpan.h>
#include <USBHIDKeyboard.h>

extern USBHIDKeyboard Keyboard;

// Macro Opcodes
#define MACRO_OP_END 0x00
#define MACRO_OP_PRESS 0x01
#define MACRO_OP_RELEASE 0x02
#define MACRO_OP_DELAY 0x03

class MacroButton {
  int pin;
  int index;
  SpanCharacteristic *switchEvent;

  // Debounce & Multi-click state for HomeKit mode
  unsigned long lastPressTime = 0;
  unsigned long pressStartTime = 0;
  unsigned long lastReleaseTime = 0;
  bool isPressed = false;
  bool isLongPressTriggered = false;
  int clickCount = 0;
  bool waitingForDouble = false;

  // Macro Playback State
  bool isPlayingMacro = false;
  int macroPtr = 0;
  unsigned long waitStart = 0;
  unsigned long waitDuration = 0;

public:
  MacroButton(int pin, int index, SpanCharacteristic *switchEvent = NULL)
      : pin(pin), index(index), switchEvent(switchEvent) {
    pinMode(pin, INPUT_PULLUP);
  }

  void loop() {
    handleMacroPlayback(); // Run macro logic every loop

    ButtonConfig &cfg = globalConfig.config.buttons[index];
    bool currentState = digitalRead(pin) == LOW; // Active LOW
    unsigned long now = millis();

    // 1. Long Press / Hold Detection for HomeKit mode while button is held
    if (cfg.type == BUTTON_HOMEKIT && isPressed && !isLongPressTriggered) {
      if (now - pressStartTime >= 450) { // Held for >= 450ms
        isLongPressTriggered = true;
        waitingForDouble = false;
        clickCount = 0;
        if (switchEvent != NULL) {
          switchEvent->setVal(2); // HomeKit LONG PRESS (Hold)
        }
      }
    }

    // 2. Double Click Timeout / Single Click Finalizer for HomeKit Mode
    if (cfg.type == BUTTON_HOMEKIT && waitingForDouble) {
      if (now - lastReleaseTime > 250) { // 250ms timeout after first click
        waitingForDouble = false;
        if (clickCount == 1 && switchEvent != NULL) {
          switchEvent->setVal(0); // HomeKit SINGLE PRESS
        }
        clickCount = 0;
      }
    }

    // 3. State Change Detection (Press / Release)
    if (currentState != isPressed) {
      if (currentState) {
        // PRESSED (Button down)
        if (now - lastPressTime > 40) { // Debounce 40ms
          isPressed = true;
          pressStartTime = now;
          isLongPressTriggered = false;
          lastPressTime = now;
          onPress();
        }
      } else {
        // RELEASED (Button up)
        if (now - lastPressTime > 40) {
          isPressed = false;
          onRelease();
          lastPressTime = now;

          if (cfg.type == BUTTON_HOMEKIT) {
            if (!isLongPressTriggered) {
              clickCount++;
              lastReleaseTime = now;

              if (clickCount == 2) {
                waitingForDouble = false;
                clickCount = 0;
                if (switchEvent != NULL) {
                  switchEvent->setVal(1); // HomeKit DOUBLE PRESS
                }
              } else {
                waitingForDouble = true;
              }
            }
            isLongPressTriggered = false;
          }
        }
      }
    }
  }

  void onPress() {
    ButtonConfig &cfg = globalConfig.config.buttons[index];

    if (cfg.type == BUTTON_HID) {
      // HID Keyboard press with modifiers
      KeyReport report = {0};
      report.modifiers = cfg.modifiers;
      report.keys[0] = (uint8_t)cfg.value;
      Keyboard.sendReport(&report);
    } else if (cfg.type == BUTTON_MACRO) {
      startMacro(cfg.value);
    }
  }

  void onRelease() {
    ButtonConfig &cfg = globalConfig.config.buttons[index];

    if (cfg.type == BUTTON_HID) {
      // Release key
      Keyboard.releaseAll();
    }
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

        KeyReport report = {0};
        report.modifiers = mod;
        report.keys[0] = key;
        Keyboard.sendReport(&report);
        break;
      }

      case MACRO_OP_RELEASE: {
        if (macroPtr + 2 > MAX_MACRO_BYTES) {
          isPlayingMacro = false;
          break;
        }
        uint8_t mod = globalConfig.config.macroBuffer[macroPtr++];
        uint8_t key = globalConfig.config.macroBuffer[macroPtr++];
        Keyboard.releaseAll();
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

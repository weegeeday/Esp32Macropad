#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include <Arduino.h>
#include <USBHIDConsumerControl.h>

extern USBHIDConsumerControl ConsumerControl;

class RotaryEncoder {
  int pinA, pinB, pinSW;
  int lastStateA;
  unsigned long lastButtonPress = 0;
  bool buttonPressed = false;

public:
  RotaryEncoder(int a, int b, int sw) : pinA(a), pinB(b), pinSW(sw) {}

  void begin() {
    pinMode(pinA, INPUT_PULLUP);
    pinMode(pinB, INPUT_PULLUP);
    pinMode(pinSW, INPUT_PULLUP);
    lastStateA = digitalRead(pinA);
  }

  void poll() {
    // Rotation
    int currentStateA = digitalRead(pinA);
    if (currentStateA != lastStateA && currentStateA == 1) {
      if (digitalRead(pinB) != currentStateA) {
        // Clockwise -> Volume Up
        ConsumerStep(CONSUMER_CONTROL_VOLUME_INCREMENT);
      } else {
        // Counter-Clockwise -> Volume Down
        ConsumerStep(CONSUMER_CONTROL_VOLUME_DECREMENT);
      }
    }
    lastStateA = currentStateA;

    // Button
    bool swState = digitalRead(pinSW) == LOW; // Active LOW
    unsigned long now = millis();

    if (swState && !buttonPressed && (now - lastButtonPress > 50)) {
      // Press
      buttonPressed = true;
      lastButtonPress = now;
      ConsumerStep(CONSUMER_CONTROL_MUTE);
    } else if (!swState && buttonPressed && (now - lastButtonPress > 50)) {
      // Release
      buttonPressed = false;
      lastButtonPress = now;
    }
  }

private:
  void ConsumerStep(uint16_t usage) {
    ConsumerControl.press(usage);
    // Wait for it to be sent (simple way)
    delay(10);
    ConsumerControl.release();
  }
};

#endif

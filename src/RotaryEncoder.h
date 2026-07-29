#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include <Arduino.h>
#include <USBHIDConsumerControl.h>

extern USBHIDConsumerControl ConsumerControl;

class RotaryEncoder {
  int pinA, pinB, pinSW;
  int lastStateA;

public:
  RotaryEncoder(int a, int b, int sw) : pinA(a), pinB(b), pinSW(sw) {}

  void begin() {
    pinMode(pinA, INPUT_PULLUP);
    pinMode(pinB, INPUT_PULLUP);
    pinMode(pinSW, INPUT_PULLUP);
    lastStateA = digitalRead(pinA);
  }

  void poll() {
    // Quadrature Rotation Processing
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
  }

private:
  void ConsumerStep(uint16_t usage) {
    ConsumerControl.press(usage);
    delay(10);
    ConsumerControl.release();
  }
};

#endif

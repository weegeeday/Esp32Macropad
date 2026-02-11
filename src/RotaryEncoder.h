#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>

extern Adafruit_USBD_HID usb_hid;

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
        ConsumerStep(HID_USAGE_CONSUMER_VOLUME_INCREMENT);
      } else {
        // Counter-Clockwise -> Volume Down
        ConsumerStep(HID_USAGE_CONSUMER_VOLUME_DECREMENT);
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
      ConsumerStep(HID_USAGE_CONSUMER_MUTE);
    } else if (!swState && buttonPressed && (now - lastButtonPress > 50)) {
      // Release
      buttonPressed = false;
      lastButtonPress = now;
    }
  }

private:
  void ConsumerStep(uint16_t usage) {
    if (!usb_hid.ready())
      return;
    usb_hid.sendReport16(
        0, usage); // report_id 0 is reserved?? No, usually fine if defined.
    // Wait for it to be sent (simple way)
    delay(10);
    usb_hid.sendReport16(0, 0); // Release
  }
};

#endif

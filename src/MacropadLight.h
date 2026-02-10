#ifndef MACROPAD_LIGHT_H
#define MACROPAD_LIGHT_H

#include <Adafruit_NeoPixel.h>
#include <HomeSpan.h>


extern Adafruit_NeoPixel pixels;
extern void setPixelColor(uint8_t r, uint8_t g, uint8_t b, int pixelIdx);

class MacropadLight : public Service::LightBulb {
  SpanCharacteristic *power;
  SpanCharacteristic *H;
  SpanCharacteristic *S;
  SpanCharacteristic *V;

public:
  MacropadLight() : Service::LightBulb() {
    power = new Characteristic::On(true);
    H = new Characteristic::Hue(0);
    S = new Characteristic::Saturation(100);
    V = new Characteristic::Brightness(50);
  }

  boolean update() override {
    boolean p = power->getNewVal();
    float h = H->getNewVal<float>();
    float s = S->getNewVal<float>();
    float v = V->getNewVal<float>();

    if (p) {
      // Convert HSV to RGB for NeoPixel
      // TinyUSB/NeoPixel library might expect RGB, HomeSpan provides HSV
      // Simple conversion:
      uint32_t rgb = pixels.ColorHSV((uint16_t)(h * 65536.0 / 360.0),
                                     (uint8_t)(s * 2.55), (uint8_t)(v * 2.55));

      // Extract RGB
      uint8_t r = (rgb >> 16) & 0xFF;
      uint8_t g = (rgb >> 8) & 0xFF;
      uint8_t b = rgb & 0xFF;

      setPixelColor(r, g, b, -1); // Set all pixels
    } else {
      setPixelColor(0, 0, 0, -1); // Turn off
    }

    return true;
  }
};

#endif

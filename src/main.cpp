#include <Arduino.h>
#include <HomeSpan.h>
#include <USB.h>
#include <USBHIDConsumerControl.h>
#include <USBHIDKeyboard.h>
#include <Update.h>
#include "esp_system.h"

#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
#include "soc/rtc_cntl_reg.h"
#endif

#include "ConfigManager.h"
#include "MacroButton.h"
#include "RotaryEncoder.h"

// Pin Definitions

// Rotary Encoder
#define PIN_ENC_A 5
#define PIN_ENC_B 10
#define PIN_ENC_SW 4

// Buttons
const int buttonPins[8] = {34, 35, 36, 37, 38, 41, 39, 40};

ConfigManager globalConfig;
USBHIDKeyboard Keyboard;
USBHIDConsumerControl ConsumerControl;
RotaryEncoder encoder(PIN_ENC_A, PIN_ENC_B, PIN_ENC_SW);
MacroButton *buttons[9];

// OTA Firmware Upload State
bool isOtaMode = false;
size_t otaTotalSize = 0;
size_t otaReceivedSize = 0;
unsigned long otaLastByteTime = 0;

String xorDecrypt(String hexInput);
void rebootToBootloader();
void handleOtaStream();

void pinsetup() {
  // Rotary Encoder
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);

  // Buttons Matrix handled by MacroButton class, but initialized here ensures pullups are set early
  for (int i = 0; i < 8; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
}

void setup() {
  Serial.begin(115200);

  // HID Setup
  Keyboard.begin();
  ConsumerControl.begin();
  USB.begin();

  // Config Setup
  globalConfig.begin();

  pinsetup();

  // Disable HomeSpan's built-in Serial CLI so it doesn't intercept custom serial commands
  homeSpan.setSerialInputDisable(true);
  homeSpan.setPairingCode("46637726");
  homeSpan.setControlPin(0); // Disable control pin to prevent floating-pin resets on boot

  // Disable WiFi modem sleep EVERY TIME WiFi connects (including on reboot)
  homeSpan.setWifiCallbackAll([](int status) {
    WiFi.setSleep(false);
    Serial.printf("OK: WiFi Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  });

  // Initialize HomeSpan
  homeSpan.begin(Category::Bridges, "Macropad Bridge", "macropad-bridge");

  // Main Bridge Accessory
  new SpanAccessory();
  new Service::AccessoryInformation();
  new Characteristic::Identify();
  new Characteristic::Name("Macropad Bridge");
  new Characteristic::Manufacturer("Espressif");
  new Characteristic::Model("ESP32-S2 Bridge");
  new Characteristic::SerialNumber("MP-BRIDGE-01");
  new Characteristic::FirmwareRevision("1.0.0");

  // Dynamically create Bridged Accessories ONLY for buttons configured in HomeKit mode
  for (int i = 0; i < 9; i++) {
    int pin = (i == 8) ? PIN_ENC_SW : buttonPins[i];
    SpanCharacteristic *switchEvent = NULL;

    if (globalConfig.config.buttons[i].type == BUTTON_HOMEKIT) {
      new SpanAccessory();
      new Service::AccessoryInformation();
      new Characteristic::Identify();
      String btnName = (i == 8) ? "Encoder Click" : ("Macropad Button " + String(i + 1));
      new Characteristic::Name(btnName.c_str());
      new Characteristic::Manufacturer("Espressif");
      new Characteristic::Model((i == 8) ? "Encoder Button" : "Macropad Button");
      String sn = "MP-BTN-0" + String(i + 1);
      new Characteristic::SerialNumber(sn.c_str());
      new Characteristic::FirmwareRevision("1.0.0");

      new Service::StatelessProgrammableSwitch();
      switchEvent = new Characteristic::ProgrammableSwitchEvent();
    }

    buttons[i] = new MacroButton(pin, i, switchEvent);
  }

  encoder.begin();
}

void loop() {
  homeSpan.poll();
  encoder.poll();

  for (int i = 0; i < 9; i++) {
    if (buttons[i])
      buttons[i]->loop();
  }

  // Handle OTA Flashing Stream
  if (isOtaMode) {
    handleOtaStream();
    return;
  }

  // Check config updates from Serial
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.startsWith("START_OTA")) {
      // START_OTA <size>
      int space = cmd.indexOf(' ');
      if (space > 0) {
        otaTotalSize = (size_t)cmd.substring(space + 1).toInt();
        if (otaTotalSize > 0 && Update.begin(otaTotalSize)) {
          isOtaMode = true;
          otaReceivedSize = 0;
          otaLastByteTime = millis();
          Serial.println("OK: OTA_READY");
        } else {
          Serial.println("ERR: OTA_BEGIN_FAILED");
        }
      } else {
        Serial.println("ERR: Invalid Format");
      }
    } else if (cmd.equalsIgnoreCase("REBOOT_BOOTLOADER")) {
      rebootToBootloader();
    } else if (cmd.startsWith("SET_WIFI")) {
      // SET_WIFI <HEX_SSID> <HEX_PASS>
      int firstSpace = cmd.indexOf(' ');
      int secondSpace = cmd.indexOf(' ', firstSpace + 1);

      if (firstSpace > 0 && secondSpace > 0) {
        String hexSSID = cmd.substring(firstSpace + 1, secondSpace);
        String hexPass = cmd.substring(secondSpace + 1);

        String ssid = xorDecrypt(hexSSID);
        String pass = xorDecrypt(hexPass);

        if (ssid.length() > 0 && pass.length() > 0) {
          homeSpan.setWifiCredentials(ssid.c_str(), pass.c_str());
          Serial.println("OK: WiFi Credentials Set");
        } else {
          Serial.println("ERR: Decrypt Failed");
        }
      } else {
        Serial.println("ERR: Invalid Format");
      }
    } else {
      globalConfig.processSerialCommand(cmd);
    }
  }
}

void handleOtaStream() {
  uint8_t buf[512];
  while (Serial.available() > 0) {
    int toRead = min(Serial.available(), (int)sizeof(buf));
    int readBytes = Serial.readBytes((char *)buf, toRead);
    if (readBytes > 0) {
      otaLastByteTime = millis();
      size_t written = Update.write(buf, readBytes);
      if (written != (size_t)readBytes) {
        Serial.println("ERR: OTA_WRITE_FAILED");
        Update.abort();
        isOtaMode = false;
        return;
      }
      otaReceivedSize += written;

      // Report progress periodically
      Serial.printf("OK: OTA_PROGRESS %u/%u\n", otaReceivedSize, otaTotalSize);

      if (otaReceivedSize >= otaTotalSize) {
        if (Update.end(true)) {
          Serial.println("OK: OTA_SUCCESS Rebooting...");
          delay(500);
          ESP.restart();
        } else {
          Serial.println("ERR: OTA_END_FAILED");
        }
        isOtaMode = false;
        return;
      }
    }
  }

  // Timeout protection: if no bytes arrive for 10 seconds, abort OTA mode
  if (millis() - otaLastByteTime > 10000) {
    Serial.println("ERR: OTA_TIMEOUT");
    Update.abort();
    isOtaMode = false;
  }
}

void rebootToBootloader() {
  Serial.println("OK: Rebooting to ROM Bootloader Mode...");
  delay(300);
#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
  REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
#endif
  esp_restart();
}

String xorDecrypt(String hexInput) {
  String output = "";
  if (hexInput.length() % 2 != 0)
    return "";

  for (int i = 0; i < hexInput.length(); i += 2) {
    String byteString = hexInput.substring(i, i + 2);
    char c = (char)strtol(byteString.c_str(), NULL, 16);
    output += (char)(c ^ 0xAA);
  }
  return output;
}
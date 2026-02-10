#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>

#define MAX_BUTTONS 8

enum ButtonType { BUTTON_HID = 0, BUTTON_HOMEKIT = 1, BUTTON_MACRO = 2 };

struct ButtonConfig {
  uint8_t type;     // ButtonType enum
  uint16_t value;   // HID usage code or HK index
  uint8_t color[3]; // RGB color for this button
};

struct GlobalConfig {
  ButtonConfig buttons[MAX_BUTTONS];
  uint8_t baseColor[3]; // Base background color for strip
};

#include <Preferences.h>

class ConfigManager {
public:
  GlobalConfig config;
  Preferences prefs;

  void begin() {
    prefs.begin("macropad", false); // Namespace "macropad", read-write
    if (!load()) {
      loadDefaults();
      save();
    }
  }

  void loadDefaults() {
    // Default: All Green, mix of HID and HK for testing
    config.baseColor[0] = 0;
    config.baseColor[1] = 10;
    config.baseColor[2] = 0;

    for (int i = 0; i < MAX_BUTTONS; i++) {
      config.buttons[i].type = (i % 2 == 0) ? BUTTON_HID : BUTTON_HOMEKIT;
      config.buttons[i].value = i;
      config.buttons[i].color[0] = 0;
      config.buttons[i].color[1] = 50;
      config.buttons[i].color[2] = 0;
    }

    // Example: Button 0 is HID 'A' (Usage 0x04)
    config.buttons[0].value = 0x04;
  }

  bool load() {
    if (prefs.getBytesLength("config") != sizeof(GlobalConfig)) {
      return false;
    }
    prefs.getBytes("config", &config, sizeof(GlobalConfig));
    return true;
  }

  void save() { prefs.putBytes("config", &config, sizeof(GlobalConfig)); }

  void processSerialCommand(String cmd) {
    cmd.trim();
    if (cmd.length() == 0)
      return;

    // CMD: RESET
    if (cmd.equalsIgnoreCase("RESET")) {
      loadDefaults();
      save();
      Serial.println("OK: Reset to defaults");
      return;
    }

    // CMD: SET_BTN <idx> TYPE <type> VAL <val> R <r> G <g> B <b>
    if (cmd.startsWith("SET_BTN")) {
      int args[6]; // idx, type, val, r, g, b
      int scanned =
          sscanf(cmd.c_str(), "SET_BTN %d TYPE %d VAL %d R %d G %d B %d",
                 &args[0], &args[1], &args[2], &args[3], &args[4], &args[5]);

      if (scanned == 6) {
        int idx = args[0];
        if (idx >= 0 && idx < MAX_BUTTONS) {
          config.buttons[idx].type = (uint8_t)args[1];
          config.buttons[idx].value = (uint16_t)args[2];
          config.buttons[idx].color[0] = (uint8_t)args[3];
          config.buttons[idx].color[1] = (uint8_t)args[4];
          config.buttons[idx].color[2] = (uint8_t)args[5];
          save();
          Serial.printf("OK: Updated Btn %d\n", idx);
        } else {
          Serial.println("ERR: Invalid Index");
        }
      } else {
        Serial.println("ERR: Invalid Format");
      }
      return;
    }

    Serial.println("ERR: Unknown Command");
  }
};

extern ConfigManager globalConfig;

#endif

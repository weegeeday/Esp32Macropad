#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>

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

    // CMD: GET_CONFIG
    if (cmd.equalsIgnoreCase("GET_CONFIG")) {
      printConfig();
      return;
    }

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

    // CMD: SET_BASE R <r> G <g> B <b>
    if (cmd.startsWith("SET_BASE")) {
      int r, g, b;
      int scanned = sscanf(cmd.c_str(), "SET_BASE R %d G %d B %d", &r, &g, &b);
      if (scanned == 3) {
        config.baseColor[0] = (uint8_t)r;
        config.baseColor[1] = (uint8_t)g;
        config.baseColor[2] = (uint8_t)b;
        save();
        Serial.println("OK: Updated Base Color");
      }
      return;
    }

    Serial.println("ERR: Unknown Command");
  }

  void printConfig() {
    JsonDocument doc;

    JsonArray baseColor = doc["baseColor"].to<JsonArray>();
    baseColor.add(config.baseColor[0]);
    baseColor.add(config.baseColor[1]);
    baseColor.add(config.baseColor[2]);

    JsonArray buttons = doc["buttons"].to<JsonArray>();
    for (int i = 0; i < MAX_BUTTONS; i++) {
      JsonObject btn = buttons.add<JsonObject>();
      btn["type"] = config.buttons[i].type;
      btn["value"] = config.buttons[i].value;

      JsonArray btnColor = btn["color"].to<JsonArray>();
      btnColor.add(config.buttons[i].color[0]);
      btnColor.add(config.buttons[i].color[1]);
      btnColor.add(config.buttons[i].color[2]);
    }

    serializeJson(doc, Serial);
    Serial.println(); // Add newline for simpler parsing
  }
};

extern ConfigManager globalConfig;

#endif

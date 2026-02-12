#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>

#define MAX_BUTTONS 8
#define MAX_MACRO_BYTES 1024

enum ButtonType { BUTTON_HID = 0, BUTTON_HOMEKIT = 1, BUTTON_MACRO = 2 };

struct ButtonConfig {
  uint8_t type;      // ButtonType enum
  uint16_t value;    // HID usage code or Macro Index
  uint8_t modifiers; // Modifier bitmap (Ctrl=1, Shift=2, Alt=4, Gui=8)
  uint8_t color[3];  // RGB color for this button
};

struct GlobalConfig {
  ButtonConfig buttons[MAX_BUTTONS];
  uint8_t baseColor[3];                 // Base background color for strip
  uint8_t macroBuffer[MAX_MACRO_BYTES]; // Storage for macro sequences
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
    // Zero out everything first
    memset(&config, 0, sizeof(GlobalConfig));

    // Default: All Green, mix of HID and HK for testing
    config.baseColor[0] = 0;
    config.baseColor[1] = 10;
    config.baseColor[2] = 0;

    for (int i = 0; i < MAX_BUTTONS; i++) {
      // Default to HID for all for easier initial testing, or keep mix?
      // Let's keep the mix but ensure valid HID codes are set for all just in
      // case.
      config.buttons[i].type = (i % 2 == 0) ? BUTTON_HID : BUTTON_HOMEKIT;

      // 0x04 is 'a', 0x05 is 'b', etc.
      config.buttons[i].value = 0x04 + i;
      config.buttons[i].modifiers = 0;

      config.buttons[i].color[0] = 0;
      config.buttons[i].color[1] = 50;
      config.buttons[i].color[2] = 0;
    }

    // Default simple macro at index 0 (if we used it)
    // For now macroBuffer is empty/zeros
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

    // CMD: SET_BTN <idx> TYPE <type> VAL <val> MOD <mod> R <r> G <g> B <b>
    if (cmd.startsWith("SET_BTN")) {
      // Try parsing with MOD first (new format)
      int args[7]; // idx, type, val, mod, r, g, b
      int scanned = sscanf(
          cmd.c_str(), "SET_BTN %d TYPE %d VAL %d MOD %d R %d G %d B %d",
          &args[0], &args[1], &args[2], &args[3], &args[4], &args[5], &args[6]);

      if (scanned == 7) {
        int idx = args[0];
        if (idx >= 0 && idx < MAX_BUTTONS) {
          config.buttons[idx].type = (uint8_t)args[1];
          config.buttons[idx].value = (uint16_t)args[2];
          config.buttons[idx].modifiers = (uint8_t)args[3]; // [NEW]
          config.buttons[idx].color[0] = (uint8_t)args[4];
          config.buttons[idx].color[1] = (uint8_t)args[5];
          config.buttons[idx].color[2] = (uint8_t)args[6];
          save();
          Serial.printf("OK: Updated Btn %d (w/ Mods)\n", idx);
        } else {
          Serial.println("ERR: Invalid Index");
        }
        return;
      }

      // Fallback for old format (without MOD)
      // CMD: SET_BTN <idx> TYPE <type> VAL <val> R <r> G <g> B <b>
      scanned =
          sscanf(cmd.c_str(), "SET_BTN %d TYPE %d VAL %d R %d G %d B %d",
                 &args[0], &args[1], &args[2], &args[4], &args[5], &args[6]);

      if (scanned == 6) {
        int idx = args[0];
        if (idx >= 0 && idx < MAX_BUTTONS) {
          config.buttons[idx].type = (uint8_t)args[1];
          config.buttons[idx].value = (uint16_t)args[2];
          config.buttons[idx].modifiers = 0; // Default text
          config.buttons[idx].color[0] = (uint8_t)args[4];
          config.buttons[idx].color[1] = (uint8_t)args[5];
          config.buttons[idx].color[2] = (uint8_t)args[6];
          save();
          Serial.printf("OK: Updated Btn %d (No Mods)\n", idx);
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

    // CMD: SET_MACRO <offset> <hex_data>
    // Example: SET_MACRO 0 01020304...
    if (cmd.startsWith("SET_MACRO")) {
      int offset;
      int space1 = cmd.indexOf(' ');
      int space2 = cmd.indexOf(' ', space1 + 1);

      if (space1 > 0 && space2 > 0) {
        String offStr = cmd.substring(space1 + 1, space2);
        offset = offStr.toInt();
        String data = cmd.substring(space2 + 1);

        if (offset >= 0 && offset < MAX_MACRO_BYTES) {
          // Convert Hex to Bytes
          int len = data.length();
          int bytesWritten = 0;
          for (int i = 0; i < len; i += 2) {
            if (offset + bytesWritten >= MAX_MACRO_BYTES)
              break; // Overflow protection

            String byteStr = data.substring(i, i + 2);
            config.macroBuffer[offset + bytesWritten] =
                (uint8_t)strtol(byteStr.c_str(), NULL, 16);
            bytesWritten++;
          }
          save();
          Serial.printf("OK: Wrote %d bytes to macro buffer at %d\n",
                        bytesWritten, offset);
        } else {
          Serial.println("ERR: Invalid Offset");
        }
      } else {
        Serial.println("ERR: Invalid Format");
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
      btn["modifiers"] = config.buttons[i].modifiers;

      JsonArray btnColor = btn["color"].to<JsonArray>();
      btnColor.add(config.buttons[i].color[0]);
      btnColor.add(config.buttons[i].color[1]);
      btnColor.add(config.buttons[i].color[2]);
    }

    // We don't print the huge macro buffer by default in JSON to save
    // time/space But maybe we return a checksum or usage? For now skip.

    serializeJson(doc, Serial);
    Serial.println(); // Add newline for simpler parsing
  }
};

extern ConfigManager globalConfig;

#endif

# ESP32-S2 Hybrid Macropad (HID + HomeKit)
# Assisted/Created by Google Antigravity

This project is a custom 8-key Macropad + Rotary Encoder built on the **ESP32-S2** platform. It combines **USB-HID** keyboard functionality with **Apple HomeKit** smart home control, allowing each button to be dynamically configured as either a macro key or a smart home trigger.

## Features

*   **Hybrid Operation**: Each of the 8 buttons can be individually set to:
    *   **HID Mode**: Sends standard USB Keyboard keystrokes (e.g., 'a', 'b', media keys).
    *   **HomeKit Mode**: Acts as a "Programmable Switch" in Apple HomeKit (Single Press).
*   **Dynamic Configuration**:
    *   Update button mappings and types on-the-fly via **USB Serial** (no recompiling needed).
    *   Settings are saved to **NVS Flash** and persist across reboots.
*   **Rotary Encoder**:
    *   Currently mapped to Volume/Media control (HID).

## Hardware Pinout
| Component       | GPIO Pin(s)       | Note                                |
| :---            | :---              | :---                                |
| **Encoder**     | `5` (DT), `10` (CLK), `4` (SW) | Rotary Encoder with Pushbutton      |
| **Buttons 1-8** | `34` - `41`       | Active LOW (Internal Pull-up used)  |
| **USB**         | Native USB        | GPIO 19/20 (D-/D+) on ESP32-S2      |

## Installation & Build

### Prerequisites
*   **VS Code** with **PlatformIO** extension.
*   **ESP32-S2** board (e.g., ESP32-S2-Saola-1, Lolin S2 Mini).

### Steps
1.  Clone this repository.
2.  Open the folder in VS Code.
3.  Click the **PlatformIO Build** (checkmark) icon to compile.
    *   *Note*: The first build will download libraries (HomeSpan, Adafruit TinyUSB).
4.  Connect your board in Download Mode (hold Boot, press Reset).
5.  Click **PlatformIO Upload** (arrow) to flash.


## Configuratiom (WebApp)
Use the companion [web app](https://weegeeday.github.io/Esp32Macropad/) to configure it, using webSerial.

## HomeKit Pairing
1. Use the [web app](https://weegeeday.github.io/Esp32Macropad/) to configure Wifi settings.
2.  Open the **Home** app on iOS.
3.  Tap **Add Accessory** -> **More options...**
4.  Select **Macropad Bridge**.
5.  Enter Setup Code: `466-37-726` (Default HomeSpan code).

# ESP32-S2 Hybrid Macropad (HID + HomeKit)

This project is a custom 8-key Macropad + Rotary Encoder built on the **ESP32-S2** platform. It uniquely combines **USB-HID** keyboard functionality with **Apple HomeKit** smart home control, allowing each button to be dynamically configured as either a macro key or a smart home trigger.

## Features

*   **Hybrid Operation**: Each of the 8 buttons can be individually set to:
    *   **HID Mode**: Sends standard USB Keyboard keystrokes (e.g., 'a', 'b', media keys).
    *   **HomeKit Mode**: Acts as a "Programmable Switch" in Apple HomeKit (Single Press).
*   **Local Feedback**:
    *   **8x NeoPixel Strip**: Each button has a corresponding RGB LED.
    *   **Visual Logic**: LED turns *OFF* when pressed for instant feedback, then restores its color.
    *   **HomeKit Light**: The entire strip is also exposed as a dimmable, color-changing Lightbulb in HomeKit.
*   **Dynamic Configuration**:
    *   Update button mappings, types, and colors on-the-fly via **USB Serial** (no recompiling needed).
    *   Settings are saved to **NVS Flash** and persist across reboots.
*   **Rotary Encoder**:
    *   Currently mapped to Volume/Media control (HID).
*   **Simulation Ready**: Includes `wokwi.toml` and `diagram.json` for full simulation in VS Code.

## Hardware Pinout
| Component       | GPIO Pin(s)       | Note                                |
| :---            | :---              | :---                                |
| **NeoPixel**    | `12`              | 8-LED Strip (Data In)               |
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
    *   *Note*: The first build will download libraries (HomeSpan, Adafruit TinyUSB, NeoPixel).
4.  Connect your board in Download Mode (hold Boot, press Reset).
5.  Click **PlatformIO Upload** (arrow) to flash.

## Configuration (Serial API)

Connect to the board via a Serial Monitor (baud `115200`).

### Command Format
`SET_BTN <index> TYPE <mode> VAL <value> R <red> G <green> B <blue>`

*   **`<index>`**: Button number (0-7).
*   **`<mode>`**:
    *   `0`: **HID Mode** (Keyboard).
    *   `1`: **HomeKit Mode** (Smart Switch).
*   **`<value>`**:
    *   For HID: USB HID Usage ID (e.g., `4`='a', `5`='b', `40`='Enter').
    *   For HomeKit: Currently unused (can be 0), simply triggers the switch event.
*   **`R G B`**: LED Color (0-255).

### Examples

**1. Set Button 0 to HomeKit Mode (Red LED):**
```bash
SET_BTN 0 TYPE 1 VAL 0 R 255 G 0 B 0
```
*Result*: Button 1 in HomeKit. Pressing it triggers an automation. LED is Red.

**2. Set Button 1 to HID Mode 'Spacebar' (Blue LED):**
```bash
SET_BTN 1 TYPE 0 VAL 44 R 0 G 0 B 255
```
*Result*: Pressing Button 2 types a 'Space'. LED is Blue.

**3. Reset to Defaults:**
```bash
RESET
```

## HomeKit Pairing
1.  Open the **Home** app on iOS.
2.  Tap **Add Accessory** -> **More options...**
3.  Select **Macropad Bridge**.
4.  Enter Setup Code: `466-37-726` (Default HomeSpan code).

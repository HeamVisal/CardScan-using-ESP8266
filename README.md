# CardKey

CardKey is an ESP8266-based RFID access control project. The main sketch reads
RFID cards with an MFRC522/RC522 module, checks the UID against a user list from a
Google Apps Script endpoint, shows status on a 4-module MAX7219 LED matrix, sounds
a buzzer, logs scan results back to Google Sheets, and hosts a small local web
dashboard for recent activity.

## Project Structure

```text
.
├── CardKey.ino                    # Main access-control sketch
├── CardScan/
│   ├── CardScan.ino               # Simple RFID UID scanner sketch
│   └── RFID-RC522.png             # RC522 wiring/reference image
├── Distance/
│   └── Distance.ino               # Ultrasonic distance sensor test sketch
├── LED4x1/
│   ├── MAX7219 Dot Matrix.png     # MAX7219 wiring/reference image
│   ├── TextDisplay/TextDisplay.ino # Basic LED matrix text test
│   └── WebSetup/WebSetup.ino      # Wi-Fi LED matrix message demo
└── OS-TP.pdf                      # Project/report document
```

## Main Features

- Reads RFID card UIDs with an RC522 module.
- Loads authorized users from a Google Apps Script web app.
- Displays states such as `WIFI`, `SYNC`, `WEB`, `READY`, `SCAN`, user names, and
  `DENY` on a MAX7219 LED matrix.
- Uses buzzer patterns for scan, success, and failure feedback.
- Gets current time from NTP and applies Cambodia time, UTC+7.
- Sends scan logs to Google Sheets through HTTPS POST requests.
- Hosts a local web dashboard at the ESP8266 IP address.
- Keeps the 10 most recent scans in memory for the dashboard.

## Hardware

- ESP8266 development board, such as NodeMCU.
- MFRC522/RC522 RFID reader.
- MAX7219 LED matrix, 4 chained 8x8 modules.
- Active buzzer.
- Optional HC-SR04-style ultrasonic distance sensor for `Distance.ino`.
- Jumper wires and suitable power supply.

## Pin Mapping

### `CardKey.ino`

| Module | Signal | ESP8266 Pin |
| --- | --- | --- |
| RC522 | SDA/SS | D8 |
| RC522 | RST | D3 |
| RC522 | SPI | Default SPI bus |
| MAX7219 | DIN | D2 |
| MAX7219 | CLK | D0 |
| MAX7219 | CS | D1 |
| Buzzer | Signal | D4 |

### Helper Sketches

- `CardScan/CardScan.ino`: RC522 SDA/SS on `D2`, RST on `D1`, SPI on the default
  ESP8266 SPI pins.
- `LED4x1/TextDisplay/TextDisplay.ino`: MAX7219 CS on `D8`, using SPI.
- `LED4x1/WebSetup/WebSetup.ino`: MAX7219 CS on `D8`, using SPI and Wi-Fi.
- `Distance/Distance.ino`: ultrasonic trigger on `D3`, echo on `D4`.

## Required Arduino Libraries

Install these from the Arduino IDE Library Manager or your preferred Arduino
tooling:

- `MFRC522`
- `MD_Parola`
- `MD_MAX72XX`
- `ArduinoJson`
- ESP8266 board package, which provides:
  - `ESP8266WiFi`
  - `ESP8266WebServer`
  - `ESP8266HTTPClient`
  - `WiFiClientSecure`

## Configuration

Before uploading `CardKey.ino`, update these values in the sketch:

- `WIFI_SSID`
- `WIFI_PASS`
- `GS_BASE`

`GS_BASE` should point to a deployed Google Apps Script web app that supports:

- `GET /exec?action=users`: returns a JSON array of authorized users.
- `POST /exec`: receives scan log JSON.

Expected user JSON fields:

```json
[
  {
    "uid": "A1B2C3D4",
    "firstName": "First",
    "lastName": "Last",
    "position": "Role",
    "age": 20
  }
]
```

Scan logs are posted with fields for UID, name, position, result, time, day,
month, and year.

## Uploading

1. Install the ESP8266 board package in Arduino IDE.
2. Select the correct ESP8266 board and serial port.
3. Install the required libraries.
4. Open `CardKey.ino`.
5. Update Wi-Fi and Google Apps Script settings.
6. Upload the sketch.
7. Open Serial Monitor at `115200` baud.
8. After Wi-Fi connects, open the printed `http://<device-ip>/` address to view
   the dashboard.

## Helper Sketches

Use these sketches to test individual parts before running the full system:

- `CardScan/CardScan.ino`: prints scanned RFID UIDs to Serial Monitor.
- `LED4x1/TextDisplay/TextDisplay.ino`: displays fixed sample words on the LED
  matrix.
- `LED4x1/WebSetup/WebSetup.ino`: hosts a small page for sending scrolling text
  to the LED matrix over Wi-Fi.
- `Distance/Distance.ino`: prints ultrasonic distance readings to Serial Monitor.

## Notes

- `CardKey.ino` uses `client.setInsecure()` for HTTPS requests on ESP8266. This is
  convenient for development, but it skips certificate verification.
- The main sketch retries Wi-Fi and user sync until successful before becoming
  ready to scan cards.
- The authorized user cache is limited to `MAX_USERS`, currently 60.
- Do not commit real Wi-Fi passwords or private Apps Script URLs in shared copies
  of this project.

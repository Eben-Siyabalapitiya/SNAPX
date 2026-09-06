# SNAPX

SNAPX is a handheld digital camera built from scratch on an AI-Thinker ESP32-CAM.
It has a live preview on a 1.8 inch TFT screen, a physical metal shutter button on
the side, and a 3D printed enclosure designed in Onshape. Photos do not go to an SD
card. They are saved to the ESP32's internal flash through LittleFS, and the camera
hosts its own WiFi access point that serves a web gallery. You connect your phone to
the camera's network, open `192.168.4.1`, and get a grid of every photo on the
device with a lightbox view, download, and delete. This repository has everything
needed to rebuild it: firmware, CAD, wiring, and a parts list.

Built by Eben Siyabalapitiya. Portfolio writeup:
https://ebensiyabalapitiya.site/projects/snapx

## Demo video

TODO: paste YouTube link here

<!-- Replace the line above with the full YouTube URL once the demo video is uploaded. -->

## The finished build

![SNAPX handheld camera](docs/cover.jpg)

## Repository layout

| Path | Contents |
|---|---|
| `firmware/SnapX/SnapX.ino` | The full firmware, single Arduino sketch |
| `cad/` | STEP and STL files for the enclosure, plus print notes |
| `docs/` | Build photo and wiring diagram |

## Bill of materials

| Part | Details | Notes |
|---|---|---|
| AI-Thinker ESP32-CAM | Classic board, OV2640 2MP sensor | The board this whole project is built around |
| ESP32-CAM-MB programmer board | Type-C, CH340G USB serial | Plugs onto the ESP32-CAM for flashing and power. Not strictly required, see the flashing notes |
| 1.8 inch ST7735S SPI TFT | 128x160, 4-wire SPI | The module has its own microSD slot. It is not used in this build |
| 12mm momentary metal push button | Waterproof, prewired, 1 normally open | The side shutter button |
| 18650 cell in a Type-C power bank enclosure | 5V USB output | Powers the camera over USB. The cell lives outside the case, not inside it |
| Onboard white LED on GPIO4 | Already on the ESP32-CAM | Used as the camera flash. Nothing to buy |
| Jumper wire, solder | | For the screen and button connections |
| 3D printed enclosure | See `cad/` | Holds the board, screen, and button only |

## Wiring

A diagram is in [docs/wiring.svg](docs/wiring.svg). The tables below are the source
of truth.

Screen to board:

| Screen pin | ESP32-CAM |
|---|---|
| VCC | 3.3V |
| GND | GND |
| CS | GPIO13 |
| DC (A0) | GPIO12 |
| MOSI (SDA) | GPIO15 |
| SCK | GPIO14 |
| LED | 3.3V |
| RESET | Not wired to a GPIO. `TFT_RST` is set to -1 in firmware |

Shutter button:

| Button | Connection |
|---|---|
| One leg | GPIO3 |
| Other leg | GND |

The button uses the ESP32 internal pullup and is debounced in software, so no
external resistor is needed.

Flash:

| | |
|---|---|
| Onboard white LED | GPIO4, driven on a PWM channel |

Camera sensor. This is the standard AI-Thinker pin map. It is already handled in
firmware and listed here only so you know which pins are taken:

```
PWDN 32   RESET -1   XCLK 0   SIOD 26   SIOC 27
Y9 35   Y8 34   Y7 39   Y6 36   Y5 21   Y4 19   Y3 18   Y2 5
VSYNC 25   HREF 23   PCLK 22
```

## Flashing

Open `firmware/SnapX/SnapX.ino` in the Arduino IDE.

Board settings:

| Setting | Value |
|---|---|
| Board | AI Thinker ESP32-CAM |
| PSRAM | Enabled |
| Partition Scheme | Huge APP (3MB No OTA / 1MB SPIFFS) |

Libraries to install from the Library Manager:

- Adafruit GFX Library
- Adafruit ST7735 and ST7789 Library

`esp_camera`, `LittleFS`, `WiFi`, and `WebServer` all ship with the ESP32 Arduino
core. There is nothing else to install.

Change the WiFi name and password if you want to. They are at the top of
`firmware/SnapX/SnapX.ino` in a block marked `USER CONFIG`:

```c
#define AP_SSID     "SNAPX"
#define AP_PASSWORD "snapx1523"
```

`AP_PASSWORD` must be 8 to 63 characters for WPA2. Use `""` for an open network.

### Gotcha 1: GPIO0 has to be grounded during a reset

The classic ESP32-CAM has no USB port and no auto reset circuit. The ESP32-CAM-MB
programmer board handles this for you. If you are flashing without it, you have to
tie GPIO0 to GND before the chip resets, start the upload, and then disconnect
GPIO0 from GND after flashing so the board boots your code instead of the
bootloader.

### Gotcha 2: pick the right board profile

Select **AI Thinker ESP32-CAM** as the board. If you select the generic **ESP32 Dev
Module** profile instead, PSRAM is left disabled, and the camera then fails at init
with a frame buffer malloc error. The camera needs PSRAM for its frame buffers.

## Using it

1. Plug in the power bank. You get a splash screen, then a short boot report for
   storage, network, and sensor.
2. Point the camera and press the side button. The flash LED fires, the screen
   flashes white and freezes on the shot you just took, and it saves to flash.
3. To pull photos off the device, connect your phone or laptop to the WiFi
   network `SNAPX` with password `snapx1523`, then open `http://192.168.4.1/` in a
   browser.

The gallery is a grid of every photo on the device. Click one for a full size
lightbox with left and right navigation and a download button. Delete is the small
control in the corner of each grid tile.

## How the firmware works

**The preview path never touches JPEG.** The camera is configured for
`PIXFORMAT_RGB565` at QVGA, which is already the pixel format the ST7735 expects, so
frames go from the camera buffer straight to the display with no decode step in
between. The rotation and scaling from the 320x240 sensor frame to the 160x128 panel
run off lookup tables that are built once on the first frame, not recalculated per
pixel. This is the difference between roughly 0.1 fps and a usable preview.

**JPEG encoding happens only on shutter press.** On capture, the raw frame is run
through `frame2jpg` at quality 95 and written to LittleFS under `/photos` as
`img_00001.jpg` and up. On boot the firmware scans that directory to find the
highest existing number, so the photo count and file numbering survive power
cycles.

**Sensor tuning matters for image quality.** White balance, gain, exposure, and
lens correction are off by default on the OV2640. Turning them on makes a large
difference. Lens correction in particular fixes the dark corners.

## Why there is no SD card

This is the single most confusing design decision from the outside, so it is worth
spelling out. The OV2640 camera sensor occupies almost every usable GPIO on the
ESP32-CAM. The pins left over after the camera and the screen are the serial lines
that the USB programmer uses, and one of those is the line the shutter button is on.
There was physically nothing left for SD card chip select and MISO. So photos go to
the ESP32's internal flash through LittleFS instead, and the WiFi gallery exists to
solve the problem that this created, which is getting the files back off the device
without a card reader.

## Enclosure

The CAD is in [cad/](cad/), along with a separate README in that folder covering
which file to print and with what settings. In short: print `SNAPX-body.stl` and
`SNAPX-cover.stl` once each in PLA or PETG at 0.2 mm layer height, 3 walls, 15 to 20
percent infill, no supports if the body is printed with the screen opening facing
up. The `SNAPX-enclosure.step` file is the full design for anyone who wants to
modify it. The battery is a USB power bank that sits outside the case, so the
enclosure only has to hold the board, the screen, and the button.

## Known issues

- Saved photos come out rotated 90 degrees relative to the preview. The preview is
  rotated in software for framing, and the JPEG encoder is handed the raw sensor
  buffer, so the file and the on-screen image do not match orientation.
- Preview frame rate is capped by how fast the panel can be written over SPI. It is
  fine for framing a shot. It is not smooth video.
- The brownout detector is disabled at boot. If you power the board from something
  that sags under load, you get glitches that look like display bugs but are
  actually the supply dropping.
- The gallery lists at most 200 photos. Older photos past that count are still on
  flash and still served if you know the URL, they just do not appear in the grid.
- The gallery has no login. Anyone connected to the `SNAPX` WiFi network can view,
  download, and delete every photo. The only barrier is the WiFi password.
- There is no way to review photos on the device screen yet. You need the web
  gallery.

## License

MIT. See [LICENSE](LICENSE). Copyright Eben Siyabalapitiya.

# SNAPX enclosure

The enclosure was designed in Onshape. It holds the ESP32-CAM board, the 1.8"
TFT screen, and the metal shutter button. The battery does not go inside. Power
is a USB power bank plugged into the board from outside the case.

## Files in this folder

| File | What it is | Do you print it |
|---|---|---|
| `SNAPX-enclosure.step` | STEP export of the full Onshape Part Studio, every part in one file. Use this to open the design in another CAD program or to remix it. | No |
| `SNAPX-body.stl` | The main shell. Screen window, lens hole, button hole, opening for the USB cable. | Yes, 1x |
| `SNAPX-cover.stl` | The back cover that closes the shell. | Yes, 1x |

If your Onshape Part Studio is split into different parts than "body" and
"cover", export one STL per part, name each STL after the part, and update the
table above so it matches what is actually in the folder.

## Getting the files out of Onshape

STEP:
1. Right click the Part Studio tab at the bottom of the Onshape window.
2. Export.
3. Format: STEP. Save as `SNAPX-enclosure.step`.

STL, one per part:
1. Right click a part in the Parts list.
2. Export.
3. Format: STL, Binary, units mm. Save as `SNAPX-body.stl` / `SNAPX-cover.stl`.

Drop the exported files in this folder and delete the matching `.PLACEHOLDER`
files.

## Printing

- Material: PLA or PETG.
- Layer height: 0.2 mm.
- Walls: 3 perimeters.
- Infill: 15 to 20 percent.
- Supports: only if your screen window or button hole is bridged. Print the
  body with the screen opening facing up and you should not need any.
- Orientation: body open face down, cover flat.

After printing, test fit the board and screen before wiring anything
permanently. The screen sits behind the window in the body, the button
threads through the side hole, the USB port on the board lines up with the
cable opening.

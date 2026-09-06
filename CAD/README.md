# CAD

The case was designed in Onshape. It holds the ESP32-CAM board, the 1.8 inch TFT
screen, and the shutter button. The battery does not go inside. Power is a USB
power bank that plugs into the board from outside the case.

## Files

| File | What it is | Print it |
|---|---|---|
| `Case.stl` | The main body of the enclosure. Roughly 66 x 53 x 30 mm. Has the screen window, the lens hole, the button hole, and the slot for the USB cable. | Yes, 1x |
| `Cover.stl` | The faceted front panel that closes the case. About 3.8 mm thick. | Yes, 1x |
| `SNAPX-enclosure.step` | The full assembly in STEP format, both parts together. Open this if you want the whole thing in another CAD program. | No |
| `Case.step` | Just the main body in STEP format. | No |
| `Cover.step` | Just the front panel in STEP format. | No |

## Printing

- Material: PLA or PETG, either is fine.
- Layer height: 0.2 mm.
- Walls: 3 perimeters.
- Infill: 15 to 20 percent.
- Supports: none needed. Print `Case.stl` with the open face down and the screen
  window facing up, and print `Cover.stl` flat.

Do a test fit with the board and screen before you glue or screw anything
together. The screen sits behind the window in the body, the button threads
through the hole in the side, and the USB port on the board should line up with
the cable slot.

## Exporting from Onshape again

STEP, one file per part:

1. Right click a part in the Parts list.
2. Export.
3. Format STEP. Save as `Case.step` or `Cover.step`.

STL, one file per part:

1. Right click a part in the Parts list.
2. Export.
3. Format STL, Binary, units mm. Save as `Case.stl` or `Cover.stl`.

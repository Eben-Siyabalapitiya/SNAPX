# CAD

The case was designed in Onshape. It holds the ESP32-CAM board, the 1.8 inch TFT
screen, and the shutter button. The battery does not go inside. Power is a USB
power bank that plugs into the board from outside the case.

## Files

| File | What it is | Print it |
|---|---|---|
| `Case.stl` | The main body of the enclosure. Roughly 66 x 53 x 30 mm. Has the screen window, the lens hole, the button hole, and the slot for the USB cable. | Yes, 1x |
| `Cover.stl` | The faceted front panel that closes the case. About 3.8 mm thick. | Yes, 1x |
| `SNAPX-enclosure.step` | The full CAD assembly in STEP format, both parts in one file. This is the file to open if you want to change the design in another CAD program. | No |

If you see a `.PLACEHOLDER` file next to one of these names, that export has not
been added yet. Open the placeholder file for what to do.

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

STEP (the whole thing):

1. Right click the Part Studio tab at the bottom of the window.
2. Export.
3. Format STEP. Save it as `SNAPX-enclosure.step`.

STL (one file per part):

1. Right click a part in the Parts list.
2. Export.
3. Format STL, Binary, units mm. Save as `Case.stl` or `Cover.stl`.

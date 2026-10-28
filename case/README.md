# Beacon case

A three-part 3D-printed case for the Radioland nRF52832 beacon. It clamps onto a strip 50 mm wide and 5 mm thick, and has a flexible button in the lid.

| Part | File | What it does |
| --- | --- | --- |
| Body | `stl/body.stl` | Cup that holds the board battery-side down, on a 50 × 30 mm flange that rests on the strip |
| Lid | `stl/lid.stl` | Snaps onto the body. A tongue cut into the top, with a nub underneath, presses the board's button. A ring presses the board's edge to stop it rattling. |
| Clip | `stl/clip.stl` | U-channel under the strip. It snaps over the flange edges and clamps the strip between them. |

Together they stand 10.2 mm above the strip and 2 mm below it. The lid is 32.6 mm across.

The board pocket and notch key use the same sizes as the [jig](../jig/README.md) base, which fitted your board.

## Before printing: measure the button

The nub's position and length rest on two estimates in [beacon_case.scad](beacon_case.scad):

- **`button_h` (2.0 mm):** the height of the button's top above the PCB. The nub stops 0.3 mm above that.
  - If the button is lower, the tongue just needs a little more travel to press it.
  - If it's taller than 2.3 mm, the button is held pressed all the time. In that case raise `button_h`.
- **`button_pos` ([-0.8, -0.5] mm):** the button's offset from the board centre, with the lanyard notch towards -X. This is read off Radioland's product photo, so it may be about 0.5 mm off. The nub is 3 mm across, which leaves some margin.

## Printing

Print each part flat as exported, with no supports. The body sits on its flange, the lid on its top face, and the clip on its plate.

PETG is the better choice for the clip, because its walls flex as they snap over the flange. PLA works for the other two parts.

## Assembly

1. **Board in.** Drop the board into the body battery-side down, with its notch over the key.
2. **Lid on.** Line up the slot in the lid's rim with the rib on the body, and press until it snaps. To open it, lever it up at the notch opposite the rib.
3. **Onto the strip.** Lay the body on the strip with the strip running along the flange's 30 mm side. Put the clip under the strip and press the two together until the hooks snap over the flange edges. The posts at the clip's ends stop the body sliding along it.
   - If the clamp is too tight or too loose, adjust `strip_fit`. A positive value leaves more room; a negative one clamps harder.

## Not covered

- **LED:** its position on the board isn't known, so the lid has no window for it. A light-coloured lid may let some light through the thin tongue.
- **Sealing:** the slot around the button tongue is open, so the case isn't waterproof.

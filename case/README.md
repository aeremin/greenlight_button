# Beacon case

A three-part 3D-printed case for the Radioland nRF52832 beacon. It mounts on a strip 50 mm wide and 5 mm thick without screws or other hardware, and has a flexible button in the lid.

| Part | File | What it does |
| --- | --- | --- |
| Body | `stl/body.stl` | Cup that holds the board battery-side down, on a flange that rests on the strip. Each long edge of the flange is a 45° dovetail tongue. |
| Lid | `stl/lid.stl` | Snaps onto the body. A tongue cut into the top, with a nub underneath, presses the board's button. A ring presses the board's edge to stop it rattling. Unchanged from the previous version. |
| Clamp | `stl/clamp.stl` | Plate under the strip, with a solid rail along each edge of the strip. Each rail has a dovetail slot for one of the body's tongues. |

## How the mount holds

The body slides along the strip into the clamp's slots.

- **Up and down:** each tongue reaches 2 mm under a solid rail. The rails are 5 mm thick and don't need to flex, so the body can't be pulled up off the strip. Pressing the button pushes the body onto the strip, not on the joint.
- **Along the strip, one way:** a cap at the back of the body stops against the clamp's end.
- **Along the strip, the other way:** two spring prongs are cut into the flange edges. When the body is fully in, hooks on their tips snap out behind the clamp's front end. Pulling on the body puts the prongs in tension, and the hooks only dig in harder. They only let go when you squeeze both tips inward.
- **Rattle:** the clamp plate squeezes the strip by 0.3 mm (`strip_squeeze`), pressing the body up against the dovetails.

The rail tops sit flush with the flange, and the outer edges are chamfered. Apart from the round cup and lid, the top is one flat surface. The mount is 60.6 mm across. The clamp is 30 mm long along the strip, and the body is 37.2 mm. The case stands 10.4 mm above the strip and 2.5 mm below it.

The board pocket and notch key use the same sizes as the [jig](../jig/README.md) base, which fitted your board.

## Before printing: measure the button

The nub's position and length rest on two estimates in [beacon_case.scad](beacon_case.scad):

- **`button_h` (2.0 mm):** the height of the button's top above the PCB. The nub stops 0.3 mm above that.
  - If the button is lower, the tongue just needs a little more travel to press it.
  - If it's taller than 2.3 mm, the button is held pressed all the time. In that case raise `button_h`.
- **`button_pos` ([-0.8, -0.5] mm):** the button's offset from the board centre, with the lanyard notch towards -X. This is read off Radioland's product photo, so it may be about 0.5 mm off. The nub is 3 mm across, which leaves some margin.

## Printing

Print each part flat as exported, with no supports. The body sits on its flange, the lid on its top face, and the clamp on its plate. All dovetail faces are 45°.

- **Material:** PETG for the body and clamp. The prongs flex about 2% when they spring, which PETG takes repeatedly and PLA may crack under. PLA is fine for the lid.
- **Walls:** use at least 3 perimeters, so the prongs and rails are solid plastic rather than infill.

## Assembly

1. **Board in.** Drop the board into the body battery-side down, with its notch over the key.
2. **Lid on.** Line up the slot in the lid's rim with the rib on the body, and press until it snaps. To open it, lever it up at the notch opposite the rib.
3. **Clamp under.** Hold the clamp under the strip, rails up, so the strip sits between the rails. The strip goes in from above, so it doesn't need a free end.
4. **Body on.** Put the body on the strip next to the clamp's flared end, prong tips first. Line its tongues up with the slots and push it along the strip until both prongs click. The cap at the back should then touch the clamp.

To remove it, squeeze the two prong tips towards each other, where they stick out past the clamp, and slide the body back out.

## Tuning

Change these in [beacon_case.scad](beacon_case.scad), then re-export the body and clamp:

```bash
for p in body clamp; do openscad -D "part=\"$p\"" -o stl/$p.stl beacon_case.scad; done
```

- **Strip isn't 5 mm thick:** set `strip_t` to its real thickness.
- **Body is hard to slide in:** lower `strip_squeeze`.
- **Body rattles, or the case creeps along the strip:** raise `strip_squeeze`. Soft webbing takes more squeeze than a rigid strip.
- **Prongs too stiff or too weak:** change `hook_d`, how far the hooks stick out. The default is 1.5 mm.

## Not covered

- **Lid retention:** the lid is still held only by its snap bead. It is not secured as positively as the mount.
- **LED:** its position on the board isn't known, so the lid has no window for it. A light-coloured lid may let some light through the thin tongue.
- **Sealing:** the slot around the button tongue is open, so the case isn't waterproof.

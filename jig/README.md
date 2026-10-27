# Pogo-pin programming jig

A two-part 3D-printed jig for the Radioland nRF52832 coin-cell beacon. The board lies component side up in the base, keyed by its lanyard notch. The lid slides down two pegs and presses six pogo pins onto the pads. The pegs are different sizes, so the lid only fits one way round.

Pad positions come from [an-erd's laser-cut jig](https://github.com/an-erd/ble_beacon/tree/master/Documentation/JIG), which was built for this board, and they agree with Radioland's module diagram. The lid is engraved with the signal names VCC, GND, SWDIO and SWCLK. The two NFC pins are left unlabelled. This README uses the pad numbers from the [reflashing guide](../Reflashing%20the%20Radioland%20nRF52832%20Coin-Cell%20Beacon.md#wiring): 1 GND, 2 VCC, 3 and 4 NFC, 5 SWCLK, 6 SWDIO. The guide isn't certain which of the SWD pair is which. If the probe won't connect, swap those two wires; it's harmless.

## Pins

The model is set up for GA20-10025-0818 pins ([drawing](pogo_pin_dimensions.png)): 1.5 mm barrel, 2.0 mm flange, 9.5 mm from the plunger tip to the flange. They go in tail up, and each flange sits in a seat on top of the lid, which sets the pin height. No gauge or measuring is needed.

Pads 5 and 6 (SWCLK and SWDIO) are 1.69 mm apart, which is closer than two 1.5 mm barrels allow. Those two pins lean 5° away from each other (`pair_tilt`). Their tips still land on the pads, and the lid has a wall between the barrels, so the two pins can't touch and short.

When the lid is down, each plunger is pushed in by `pin_preload` (0.8 mm). The drawing doesn't give the pins' travel, so check that yours compress by at least 1.2 mm. If they compress less, lower `pin_preload`.

## Board height

`board_back_h` in [beacon_jig.scad](beacon_jig.scad) is set to the measured 4.0 mm. It is the height of whatever is under the PCB, which is the battery holder with the cell in, if you'll flash with it fitted. `board_t` (PCB thickness) is still a guess at 1.0 mm.

After changing a parameter, re-export:

```bash
for p in base lid hole_test; do openscad -D "part=\"$p\"" -o stl/$p.stl beacon_jig.scad; done
```

## Parts

| File | What it is |
| --- | --- |
| `stl/base.stl` | Board pocket with alignment pegs and finger slots |
| `stl/lid.stl` | Pin carrier with flange seats, engraved with signal names; the arrow points to the notch side |
| `stl/hole_test.stl` | Holes from 1.4 to 1.8 mm, for choosing `pin_hole_d` |

Print everything flat as exported, with no supports. Use a 0.2 mm layer height or finer for the lid.

## Assembly

1. **Size the holes.** Print `hole_test` and push a pin barrel into each hole. Set `pin_hole_d` (now 1.8 mm, the best fit on the first print) to the hole that grips a pin firmly but still lets it slide under thumb pressure, then re-export the lid.
2. **Fit the pins.** Push each pin in from the top of the lid until its flange sits in its seat. Pins 5 and 6 go in at their 5° lean. Add a drop of CA glue at the flanges if the fit is loose.
3. **Wire it.** Solder wires to the pin tails following the guide's wiring table. Pins 3 and 4 are optional; an-erd wires a button and an LED to them.
4. **Flash.** Drop the board in, put the lid on and press it down by hand, or loop rubber bands through the grooves.

## Unverified

- The PCB thickness (`board_t`) and the pins' travel are guesses until you measure them.
- The hole positions are taken from an-erd's design files, not measured on your board. Before you glue the pins, put the lid on with the board in place and look down the holes to check that each one lines up with its pad.

# Pogo-pin programming jig

A two-part 3D-printed jig for the Radioland nRF52832 coin-cell beacon. The board lies component side up in the base, keyed by its lanyard notch. The lid slides down two pegs and presses six pogo pins onto the pads. The pegs are different sizes, so the lid only fits one way round.

Pad positions come from [an-erd's laser-cut jig](https://github.com/an-erd/ble_beacon/tree/master/Documentation/JIG), which was built for this board, and they agree with Radioland's module diagram. The numbers engraved on the lid match the pad numbers in the [reflashing guide](../Reflashing%20the%20Radioland%20nRF52832%20Coin-Cell%20Beacon.md#wiring): 1 GND, 2 VCC, 3 and 4 NFC, 5 SWCLK, 6 SWDIO.

## Board height

`board_back_h` in [beacon_jig.scad](beacon_jig.scad) is set to the measured 4.0 mm. It is the height of whatever is under the PCB, which is the battery holder with the cell in, if you'll flash with it fitted. `board_t` (PCB thickness) is still a guess at 1.0 mm. The pins have about ±1.2 mm of slack, so a small error is fine and a large one isn't.

After changing a parameter, re-export:

```bash
for p in base lid gauge hole_test; do openscad -D "part=\"$p\"" -o stl/$p.stl beacon_jig.scad; done
```

## Parts

| File | What it is |
| --- | --- |
| `stl/base.stl` | Board pocket with alignment pegs and finger slots |
| `stl/lid.stl` | Pin carrier, engraved with pad numbers; the arrow points to the notch side |
| `stl/gauge.stl` | Spacer frame for setting the pin height |
| `stl/hole_test.stl` | Holes from 0.9 to 1.3 mm, for choosing `pin_hole_d` |

Print everything flat as exported, with no supports. Use a 0.2 mm layer height or finer for the lid.

## Assembly

1. **Size the holes.** Print `hole_test` and push a pin into each hole. Set `pin_hole_d` to the hole that grips a pin firmly but still lets it slide under thumb pressure, then re-export the lid.
2. **Fit the pins.** The defaults suit P75-series pins (1.02 mm barrel, about 16.5 mm long, such as P75-B1 or P75-E2). Push them in from the top of the lid.
3. **Set their height.** Stand the lid on `gauge` on a flat table and push each pin down until its tip touches the table. Each tip then sticks out 4.2 mm below the lid. Fix the pins with a drop of CA glue at the top.
4. **Wire it.** Solder wires to the pin tails following the guide's wiring table. Pins 3 and 4 are optional; an-erd wires a button and an LED to them.
5. **Flash.** Drop the board in, put the lid on and press it down by hand, or loop rubber bands through the grooves.

## Unverified

- The PCB thickness (`board_t`) is a guess until you measure it.
- The hole positions are taken from an-erd's design files, not measured on your board. Before you glue the pins, put the lid on with the board in place and look down the holes to check that each one lines up with its pad.

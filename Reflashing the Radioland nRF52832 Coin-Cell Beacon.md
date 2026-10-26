# Reflashing the Radioland nRF52832 Coin-Cell Beacon

Oct 26, 2026 · @Someone

## Summary

The beacon is the Radioland nRF52832 coin-cell tag. Radioland's own module diagram, posted in [parasquid/mcu-iot-sensors issue #9](https://github.com/parasquid/mcu-iot-sensors/issues/9#issuecomment-567411149), labels the pads: VCC and GND sit either side of the lanyard notch, SWDIO and SWCLK are the pair beside the SoC, and the other two are NFC antenna points. There is no reset pad, and SWD recovery doesn't need one.

Radioland's diagram shows the board a quarter turn round, with the lanyard notch on the left. Turned 90° clockwise, its SHT30, KX022, both crystals and the antenna line up with your photos, which gives this mapping:

| Pad (lanyard hole at 12 o'clock) | Signal | Confidence |
| --- | --- | --- |
| 1: left of the lanyard hole (pad with the via) | GND | High |
| 2: right of the lanyard hole | VCC | High |
| 3: left edge, middle | NFC antenna | High |
| 4: below the SoC, left | NFC antenna | High |
| 5: upper pad of the pair right of the SoC | SWCLK | High for the pair; medium on which is which |
| 6: directly below pad 5 | SWDIO | High for the pair; medium on which is which |

The diagram's SWDIO and SWCLK leader lines end close together, so the order within that pair is my reading of them. If the probe won't connect, swap those two wires; it's harmless. Before powering, confirm pads 1 and 2 against the battery contacts, since swapping VCC and GND is the one mistake that can do damage.

## Board facts

The open-source [an-erd/ble\_beacon](https://github.com/an-erd/ble_beacon) firmware was written for this exact board, and its README documents the pin use. The chip on your board is an nRF52832-QFAA, build code B0 (production revision 1, 2017 week 21), with 512 kB flash and 64 kB RAM.

| Function | nRF52832 pin / port | Notes |
| --- | --- | --- |
| SWDIO | QFN pin 26 | TP1 |
| SWDCLK | QFN pin 25 | TP2 |
| VDD | QFN pins 13 / 36 / 48 | TP3 |
| GND | QFN pin 45 + exposed pad | TP4 |
| NFC1 / NFC2 | P0.09 / P0.10 (pins 11 / 12) | Pads 3 and 4; usable as GPIO with `CONFIG_NFCT_PINS_AS_GPIOS`. The programming jig wires an extra button and LED to these. |
| I²C SCL / SDA | P0.30 / P0.31 | Sensor bus |
| Button, LED | P0.25 / P0.26 | The README's table and prose disagree on which is which; check before relying on it |
| Humidity / temperature | SHT3x at I²C 0x44 | The square part with the opening, top left |
| Accelerometer | KX022 at I²C 0x1E | Some units ship without one |

The 3×3 mm part marked "MJ8 22" is still unidentified. It may be the accelerometer or something else, such as a regulator. An I²C scan from your own firmware settles it: a KX022 answers at 0x1E.

The two angled solder blobs (top right and bottom left) are the CR2032 contacts, and they are the easiest reference points for battery + and GND.

## Verify before powering

With the coin cell removed, these checks pin down every pad in a few minutes.

1. **GND:** check continuity from the battery-negative contact to pad 1. The 32 MHz crystal's case and the large copper pour are also GND.
2. **VCC:** check continuity from the battery-positive contact to pad 2. Expect near 0 Ω, or a few ohms through a protection part.
3. **No short:** measure between pads 1 and 2. Expect kΩ to MΩ, rising as the capacitors charge, never 0 Ω.
4. **SWD pair:** with a needle probe, check continuity from QFN pin 25 (SWDCLK, bottom pin of the right-hand row) and pin 26 (SWDIO, the one above it) to pads 5 and 6. This also settles which of the two is which.
5. **Diode mode cross-check:** red lead on GND, black on each signal pad. Real I/O pads read roughly 0.4–0.7 V through the chip's ESD diodes.
6. **NFC pads:** pads 3 and 4 should connect to QFN pins 11 and 12, at the bottom of the left-hand row.

## Wiring

Use a probe that can reach the nRF52's CTRL-AP: a J-Link, a CMSIS-DAP probe such as a Raspberry Pi Pico running debugprobe, or a Black Magic Probe. A stock ST-Link V2 can't run the recover command; it only works after being reflashed to CMSIS-DAP or BMP firmware.

| Beacon pad | Signal | J-Link | Pico debugprobe |
| --- | --- | --- | --- |
| 6 | SWDIO | SWDIO | GP3 |
| 5 | SWCLK | SWCLK | GP2 |
| 1 | GND | GND | GND |
| 2 | VCC | VTref (sense only) | 3V3 OUT |

- **Power from one source only:** either keep the CR2032 in and leave the probe's supply unconnected, or remove the cell and feed 3.0–3.3 V to the VDD pad. A J-Link only senses VTref and does not power the board.
- **Connections:** solder 30 AWG wire to the pads or press pogo pins on them. Keep leads under about 15 cm and start at an SWD clock of 1 MHz or lower.

## Flashing steps

Back up first if you can, then unlock and flash. Your rev-1 (B0) chip uses the old protection scheme, so one full erase unlocks it permanently.

1. **Connect.** With OpenOCD: `openocd -f interface/cmsis-dap.cfg -c "transport select swd" -f target/nrf52.cfg`. A good link shows `SWD DPIDR 0x2ba01477`. If it reports "AP lock engaged", readout protection is on.
2. **Back up, if unlocked.** OpenOCD: `init; halt; dump_image stock_flash.bin 0x0 0x80000; dump_image stock_uicr.bin 0x10001000 0x1000`. J-Link: `nrfjprog --readcode stock.hex --readuicr`. This is the only chance to keep the vendor firmware.
3. **Unlock and erase.** J-Link: `nrfjprog --recover`. OpenOCD: `init; nrf52_recover`. Black Magic Probe: `mon swdp_scan`, `attach 2`, `mon erase_mass`. This destroys the original firmware.
4. **Flash.** `nrfjprog --program fw.hex --chiperase --verify --reset`, or in OpenOCD `program fw.hex verify reset exit`. For SoftDevice-based builds, flash S132 plus the application, or a merged hex.

A locked chip can only be dumped with a voltage-glitch attack on the DEC1 supply. atc1441's ESP32\_nRF52\_SWD automates it, but for most people it's not worth the effort.

## Firmware options

Since the board is confirmed, [an-erd/ble\_beacon](https://github.com/an-erd/ble_beacon) is the obvious first choice: it already supports this exact hardware.

| Option | Fit for this board |
| --- | --- |
| an-erd/ble\_beacon (nRF5 SDK 16/17, S132) | Built for it: drives the SHT3x, KX022, button and LED, advertises sensor data, supports DFU. The author measured about 13.5 µA average versus 577 µA for the stock firmware. |
| nRF Connect SDK / Zephyr | Start from `nrf52dk/nrf52832` with an overlay for I²C on P0.30/P0.31, the button and LED, and the external 32.768 kHz crystal. |
| Nordic nRF5 SDK 17.1 | The `ble_app_beacon` example, with a custom board header using the pins above. |
| Arduino (arduino-nRF5, Generic nRF52832) or Espruino | Quickest to start; you set up the pin map yourself. |

To receive the data, OpenMQTTGateway, Theengs or ESPHome on an ESP32 can decode the advertisements. Advertising in the Ruuvi RAWv2 format lets existing gateways read it without a custom decoder.

## Remaining caveats

- The SWDIO/SWCLK order within the pad pair comes from reading the vendor diagram's leader lines; continuity check 4 confirms it.
- The button and LED pin assignment (P0.25 vs P0.26) is inconsistent in the an-erd README.
- The accelerometer fitted to your unit, if any, isn't confirmed; run an I²C scan.
- Whether the stock firmware is readout-protected is only known once you connect.

## Sources

- [an-erd/ble\_beacon](https://github.com/an-erd/ble_beacon): firmware and pin documentation for the Radioland board
- [Radioland module diagram, parasquid/mcu-iot-sensors issue #9](https://github.com/parasquid/mcu-iot-sensors/issues/9#issuecomment-567411149): pad labels
- Nordic nRF52832 Product Specification: QFN48 pinout

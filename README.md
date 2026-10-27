# Greenlight button

Zephyr firmware for the Radioland nRF52832 coin-cell beacon. See the [reflashing guide](Reflashing%20the%20Radioland%20nRF52832%20Coin-Cell%20Beacon.md) for the hardware and the [jig](jig/README.md) for flashing it.

## Layout

| Path | What it is |
| --- | --- |
| `manifest/west.yml` | West manifest: upstream Zephyr v4.4.2 plus only the modules the nRF52832 and Bluetooth pairing need |
| `boards/radioland/radioland_beacon/` | Board definition (`radioland_beacon/nrf52832`) |
| `firmware/beacon/` | Beacon app: a remote that starts and stops a GoPro and a FlySight 2 over Bluetooth |
| `third_party/`, `.west/`, `.venv/` | Fetched by the setup below; gitignored |

## Setup

You need Python 3.10+, CMake, Ninja, dtc and 7-Zip on PATH. 7-Zip is only needed by the SDK installer. From the repo root, in PowerShell:

```powershell
python -m venv .venv
.venv\Scripts\Activate.ps1
pip install west
west init -l manifest
west update --narrow -o=--depth=1
west packages pip --install
west sdk install -t arm-zephyr-eabi
west zephyr-export
```

If `ZEPHYR_BASE` is set globally (e.g. for another project), west uses that workspace instead of this one. Clear it in the shell first: `Remove-Item Env:ZEPHYR_BASE`.

## Build and flash

```powershell
west build -b radioland_beacon firmware/beacon
west flash
```

`west flash` uses nrfjprog (J-Link) by default. On a fresh beacon, run `nrfjprog --recover` once first, as the guide describes. For a CMSIS-DAP probe such as a Pico running debugprobe, use OpenOCD:

```powershell
west flash -r openocd
```

Logs go to SEGGER RTT, because the board has no UART pads.

## Using the button

The beacon is a Bluetooth remote for a GoPro (written for the HERO13 and the Open GoPro BLE API) and a FlySight 2. It talks to both at once and only to devices it has paired with. A device that was never paired is skipped.

| Press | What happens |
| --- | --- |
| Short (under 1 s) | GoPro wakes and starts recording. FlySight starts logging a GPS track (active mode). |
| Long (1 to 10 s; the LED goes dark at 1 s) | GoPro stops recording and goes to sleep. FlySight stops logging and goes to sleep. |
| Hold 10 s | Pairing mode for 60 s. Any press ends it early. |

While it works, the LED blinks briefly once a second. At the end, one long flash means every paired device did what was asked, and five quick flashes mean at least one didn't. Each device gets 30 s to show up. A newer press replaces one still running.

### Pairing

Enter pairing mode on the beacon (the LED double-blinks), then put each device in its own pairing mode within the 60 s:

- **GoPro:** Preferences → Wireless Connections → Connect Device → GoPro Quik App.
- **FlySight 2:** while it's off, press its button twice quickly. The green LED pulses.

The beacon flashes once for each device it bonds with. Pairing a new device of the same kind replaces the old one. Bonds survive battery changes.

### Requirements and limits

- **FlySight 2** needs firmware v2026.05.13.develop or later, the first with the Device State service's mode commands. Older firmware pairs but can't be started or stopped. The beacon puts it in active mode even if `FLYSIGHT.TXT` selects start (starter pistol) mode.
- **GoPro** advertises for 8 hours after it goes to sleep. After that, the camera can't be woken over Bluetooth until it's turned on by hand. A long press leaves a camera that's already asleep alone instead of waking it.

## Board notes

The pin map comes from an-erd/ble_beacon's `custom_board.h`. That file is code that ran on this hardware, so it takes precedence over the README prose, which swaps the LED and the button.

| Function | Pin | Devicetree |
| --- | --- | --- |
| LED (active high) | P0.26 | `led0` |
| Button (active low, pull-up) | P0.25 | `sw0` |
| Jig button / jig LED | P0.09 / P0.10 (NFC pads) | `sw1` / `led1` |
| I²C SCL / SDA | P0.30 / P0.31 | `i2c0`, SHT3x at 0x44 |

- Both buttons detect edges with the GPIO PORT sense mechanism (`sense-edge-mask`) instead of GPIOTE channels, which would draw current the whole time the beacon waits.
- The NFC pins are set up as GPIOs (`nfct-pins-as-gpios`). This is written to UICR on flash, so NFC needs a full erase to come back.
- DC/DC is on, because an-erd's firmware enables it, so the inductor must be fitted.
- The LF clock is the external 32.768 kHz crystal at the default 50 ppm rating. an-erd uses 20 ppm, which saves a little power once it's confirmed.
- The KX022 accelerometer (0x1E, not on every unit) has no Zephyr driver, so it isn't in the devicetree.

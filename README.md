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

- **FlySight 2** needs firmware v2026.05.13.develop or later, the first with the Device State service's mode commands. Older firmware pairs but can't be started or stopped. See [Updating the FlySight firmware](#updating-the-flysight-firmware). The beacon puts it in active mode even if `FLYSIGHT.TXT` selects start (starter pistol) mode.
- **GoPro** advertises for 8 hours after it goes to sleep. After that, the camera can't be woken over Bluetooth until it's turned on by hand. A long press leaves a camera that's already asleep alone instead of waking it.

### Updating the FlySight firmware

FlySight firmware files are encrypted for each hardware batch, so download yours from FlySight's online tool, which picks the files that match your unit. The firmware version the FlySight runs now is the `Firmware_Ver` line in `FLYSIGHT.TXT` at the root of its drive.

1. Connect the FlySight to the computer by USB.
2. Hold its button for about 10 s, until it resets and the light turns orange.
3. Open [flysight.ca/firmware/?include_beta=true](https://flysight.ca/firmware/?include_beta=true) and upload `FLYSIGHT.TXT` from the FlySight's drive. `include_beta=true` makes the tool list beta builds as well as releases.
4. Download v2026.05.13.develop or later. If the tool also offers a wireless stack update, download that too.
5. On the FlySight's drive, create a folder named `FW` if there isn't one. Copy the firmware file into it as `APP.SFB`, and the stack update, if any, as `STACK.BIN`.
6. Eject the drive, unplug the FlySight and wait a few seconds while it resets and installs.
7. Check `Firmware_Ver` in `FLYSIGHT.TXT` to confirm the new version.

If no build that new is offered for your unit, it hasn't been published for that batch yet.

## Debug build

[`debug.conf`](firmware/beacon/debug.conf) adds a shell on RTT whose `gl` commands act like the button, so the beacon can be driven without touching it. The shell polls RTT every 10 ms, so don't run this build on a coin cell.

```powershell
west build -b radioland_beacon firmware/beacon -- '-DEXTRA_CONF_FILE=debug.conf'
```

| Command | What it does |
| --- | --- |
| `gl press [ms]` | Short press, or a press held for `ms` |
| `gl long` | Long press: stop |
| `gl pair` | 10 s hold: pairing mode |
| `gl status` | Which devices are paired |
| `gl unpair <gopro\|flysight\|all>` | Forget a bond |
| `gl scan [s]` | List advertisers with their type and manufacturer ID, while idle |

The build also turns on Bluetooth pairing logs. With a Black Magic Probe, `monitor rtt enable` in GDB exposes RTT on the probe's second serial port while the target runs. Probe firmware older than v2.1 halts the nRF52 for every RTT poll, which costs radio events. Expect the odd failed connection that the beacon then retries, and slow the polling with `monitor rtt poll 3000 1000 10`.

## Board notes

The pin map comes from an-erd/ble_beacon's `custom_board.h`. That file is code that ran on this hardware, so it takes precedence over the README prose, which swaps the LED and the button.

| Function | Pin | Devicetree |
| --- | --- | --- |
| LED (active high) | P0.26 | `led0` |
| Button (active low, pull-up) | P0.25 | `sw0` |
| Jig button / jig LED | P0.09 / P0.10 (NFC pads) | `sw1` / `led1` |
| I²C SCL / SDA | P0.30 / P0.31 | `i2c0`, SHT3x at 0x44 |

- The NFC pins are set up as GPIOs (`nfct-pins-as-gpios`). This is written to UICR on flash, so NFC needs a full erase to come back.
- DC/DC is on, because an-erd's firmware enables it, so the inductor must be fitted.
- The LF clock is the external 32.768 kHz crystal at the default 50 ppm rating. an-erd uses 20 ppm, which saves a little power once it's confirmed.
- The KX022 accelerometer (0x1E, not on every unit) has no Zephyr driver, so it isn't in the devicetree.

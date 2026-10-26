# Greenlight button

Zephyr firmware for the Radioland nRF52832 coin-cell beacon. See the [reflashing guide](Reflashing%20the%20Radioland%20nRF52832%20Coin-Cell%20Beacon.md) for the hardware and the [jig](jig/README.md) for flashing it.

## Layout

| Path | What it is |
| --- | --- |
| `manifest/west.yml` | West manifest: upstream Zephyr v4.4.2 plus only the modules the nRF52832 needs |
| `boards/radioland/radioland_beacon/` | Board definition (`radioland_beacon/nrf52832`) |
| `firmware/beacon/` | Beacon app: non-connectable advertising as "Greenlight" about once a second |
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

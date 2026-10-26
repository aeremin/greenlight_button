# The guide's SWD leads are long and hand-made, so stay at 1 MHz.
board_runner_args(jlink "--device=nRF52832_xxAA" "--speed=1000")
board_runner_args(pyocd "--target=nrf52832" "--frequency=1000000")

# OpenOCD is mostly used here with a CMSIS-DAP probe such as a Pico running debugprobe.
if(NOT DEFINED OPENOCD_NRF5_INTERFACE)
  set(OPENOCD_NRF5_INTERFACE "cmsis-dap")
endif()

include(${ZEPHYR_BASE}/boards/common/nrfjprog.board.cmake)
include(${ZEPHYR_BASE}/boards/common/nrfutil.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
include(${ZEPHYR_BASE}/boards/common/pyocd.board.cmake)
include(${ZEPHYR_BASE}/boards/common/openocd-nrf5.board.cmake)
include(${ZEPHYR_BASE}/boards/common/blackmagicprobe.board.cmake)
board_runner_args(openocd --cmd-pre-init "adapter speed 1000")

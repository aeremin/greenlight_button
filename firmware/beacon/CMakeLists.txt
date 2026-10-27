cmake_minimum_required(VERSION 3.20.0)

# Custom boards live in <repo>/boards
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(beacon)

target_sources(app PRIVATE
	src/flysight.c
	src/gopro.c
	src/led.c
	src/link.c
	src/main.c
)
target_sources_ifdef(CONFIG_SHELL app PRIVATE src/shell.c)

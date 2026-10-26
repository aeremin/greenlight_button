cmake_minimum_required(VERSION 3.20.0)

# Custom boards live in <repo>/boards
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(beacon)

target_sources(app PRIVATE src/main.c)

#
# 914-control -- VCU / BMS firmware for the 1975 Porsche 914 EV conversion
#
# Ported to Mbed CE (https://github.com/mbed-ce/mbed-os) after Arm discontinued Mbed OS.
#
# Configure:  cmake -S . -B build -GNinja -DMBED_TARGET=LPC1768 -DUPLOAD_METHOD=MBED
# Build:      cmake --build build
# Flash:      cmake --build build --target flash-914-control
#
cmake_minimum_required(VERSION 3.19)
cmake_policy(VERSION 3.19...3.22)

set(APP_TARGET 914-control)

# Must be set before including the toolchain setup file.
set(MBED_APP_JSON_PATH mbed_app.json5)

### Initialise the Mbed CE build system.
include(mbed-os/tools/cmake/mbed_toolchain_setup.cmake)

project(${APP_TARGET}
        LANGUAGES C CXX ASM)

include(mbed_project_setup)

add_subdirectory(mbed-os)
add_subdirectory(lib-mbed-ltc681x)

# -Wno-register: mbed-os headers still use the `register` keyword, removed in C++17.
# -Wno-deprecated-declarations: mbed-os deprecates APIs still used here.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-register -Wno-deprecated-declarations")

add_executable(${APP_TARGET})

# Override src/config.h knobs at configure time instead of editing the file:
#
#   cmake -S . -B build-slcan ... -DCONFIG_DEFINES="SLCAN_MODE=1;SLCAN_DUAL_EMIT=1"
#
# Every knob in config.h is #ifndef-guarded, so a -D wins. Use this rather than editing
# config.h for an experiment: an edited config.h is one `git checkout` away from silently
# reverting mid-session, which has already cost one round of measurements here, and the build
# directory records which configuration produced which binary.
#
# Note this must NOT be done by passing -DCMAKE_CXX_FLAGS on the command line: that replaces the
# cache value the Mbed CE toolchain setup put there, taking -mcpu/-mthumb with it, and the build
# fails deep inside CMSIS with "Unknown Arm architecture profile".
set(CONFIG_DEFINES "" CACHE STRING
    "Semicolon-separated src/config.h overrides, e.g. SLCAN_MODE=1;SLCAN_DUAL_EMIT=1")
if(CONFIG_DEFINES)
    target_compile_definitions(${APP_TARGET} PRIVATE ${CONFIG_DEFINES})
    message(STATUS "914-control config overrides: ${CONFIG_DEFINES}")
endif()

target_sources(${APP_TARGET}
    PRIVATE
        src/Main.cpp
        src/BmsThread.cpp
        src/LTC6813.cpp
        src/MCP23017.cpp
        src/Slcan.cpp
        src/Telemetry.cpp
)

# mbed-os / mbed-baremetal are object libraries and may only be linked to executables.
# Libraries (lib-mbed-ltc681x) link mbed-core-flags instead.
if("MBED_CONF_TARGET_APPLICATION_PROFILE=bare-metal" IN_LIST MBED_CONFIG_DEFINITIONS)
    target_link_libraries(${APP_TARGET} mbed-baremetal lib-mbed-ltc681x)
else()
    target_link_libraries(${APP_TARGET} mbed-os lib-mbed-ltc681x)
endif()

# Generates bin/hex outputs and the flash-<target> rule.
mbed_set_post_build(${APP_TARGET})

option(VERBOSE_BUILD "Have a verbose build process")
if(VERBOSE_BUILD)
    set(CMAKE_VERBOSE_MAKEFILE ON)
endif()

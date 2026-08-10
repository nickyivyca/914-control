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

# lib-mbed-ltc681x still links `mbed-os`, which was correct under mbed-tools but is
# invalid under Mbed CE: mbed-os is an OBJECT library and may only be linked to
# executables. Overriding here keeps the fix inside this repo so the submodule stays
# pristine -- otherwise the patch would live only in a local working tree and be lost
# on a fresh clone. Fold this upstream into lib-mbed-ltc681x and drop this block.
set_property(TARGET lib-mbed-ltc681x PROPERTY LINK_LIBRARIES mbed-core-flags)

# -Wno-register: mbed-os headers still use the `register` keyword, removed in C++17.
# -Wno-deprecated-declarations: mbed-os deprecates APIs still used here.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-register -Wno-deprecated-declarations")

add_executable(${APP_TARGET})

target_sources(${APP_TARGET}
    PRIVATE
        src/Main.cpp
        src/BmsThread.cpp
        src/LTC6813.cpp
        src/MCP23017.cpp
        src/Slcan.cpp
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

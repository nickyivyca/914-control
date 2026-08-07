#
# Build file for the 914 VCU / BMS firmware (Mbed CE / CMake).
#
# Mbed CE replaces mbed-tools: dependencies are git submodules, not .lib files,
# and the build is plain CMake. After cloning:
#
#     git submodule update --init --recursive
#

MBED_TARGET      ?= LPC1768
UPLOAD_METHOD    ?= MBED
CMAKE_BUILD_TYPE ?= Release
BUILD_DIR        := build

.PHONY: all configure build flash clean deps

all: build

deps:
	git submodule update --init --recursive

$(BUILD_DIR):
	cmake -S . -B $(BUILD_DIR) -GNinja \
		-DMBED_TARGET=$(MBED_TARGET) \
		-DUPLOAD_METHOD=$(UPLOAD_METHOD) \
		-DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)

configure: $(BUILD_DIR)

build: configure
	cmake --build $(BUILD_DIR)

# Flashes via the interface MCU's MBED mass-storage drive.
flash: build
	cmake --build $(BUILD_DIR) --target flash-914-control

clean:
	rm -rf $(BUILD_DIR)

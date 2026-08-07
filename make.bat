@echo off
REM Build 914-control (Mbed CE / CMake). Requires cmake, ninja and arm-none-eabi-gcc on PATH.
REM Build type Develop matches what `mbed-tools compile` (no -b) produced before the Mbed CE port.
REM Valid values: Debug, Develop, Release.
cmake -S . -B build -GNinja -DMBED_TARGET=LPC1768 -DUPLOAD_METHOD=MBED -DCMAKE_BUILD_TYPE=Develop && cmake --build build

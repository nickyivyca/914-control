@echo off
REM Build 914-control and flash it via the interface MCU's MBED mass-storage drive.
REM Build type Develop matches what `mbed-tools compile` (no -b) produced before the Mbed CE port.
cmake -S . -B build -GNinja -DMBED_TARGET=LPC1768 -DUPLOAD_METHOD=MBED -DCMAKE_BUILD_TYPE=Develop && cmake --build build --target flash-914-control

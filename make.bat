@echo off
REM Build 914-control (Mbed CE / CMake). Requires cmake, ninja, arm-none-eabi-gcc on PATH.
cmake -S . -B build -GNinja -DMBED_TARGET=LPC1768 -DUPLOAD_METHOD=MBED && cmake --build build

@echo off
REM Build 914-control and flash it to the mounted MBED MSD drive.
cmake -S . -B build -GNinja -DMBED_TARGET=LPC1768 -DUPLOAD_METHOD=MBED && cmake --build build --target flash-914-control

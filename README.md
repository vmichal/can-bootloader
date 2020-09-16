
### CAN Bootloader Firmware

The target MCU is for now STM32F105RCT6 (256K flash, 64K SRAM) with an 8 MHz HSE crystal used in the Accumulator Management system.

### Project files

Project files provided are:

- CMake (additional options are required, e.g. `-DCMAKE_TOOLCHAIN_FILE=toolchain.cmake`)

### Code style

C++17 is used in the codebase. Getting a recent compiler is a must.

Motivation for C++17: **std::optional**, maybe_unused, std::byte, structured bindings

Motivation for C++20: designated initializers, std::span

Application code is found in _./Bootloader, but it would be useless without significant support in _./CANdb_, _./Drivers_, _./BSP_ as well as _./startup_.


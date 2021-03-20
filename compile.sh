#!/bin/bash

mkdir -p build
mkdir -p build/STM32F$1

cd build/STM32F$1
cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=stm32f$1.cmake ../..
ninja


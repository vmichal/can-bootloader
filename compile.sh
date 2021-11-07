#!/bin/bash

mkdir -p build
mkdir -p build/STM32$1

cd build/STM32$1
cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=stm32$1.cmake ../..
ninja


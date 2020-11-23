#!/bin/bash

rm -rf build
mkdir -p build

cd build
cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=stm32f$1.cmake ..
ninja


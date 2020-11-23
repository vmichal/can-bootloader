#!/bin/bash

mkdir -p build

cd build
cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=stm32f4.cmake ..
ninja


#!/bin/bash

if [ $1 == 'all' ] || [ $1 == 'ALL' ]
then
  ./compile.sh F1
  ./compile.sh F2
  ./compile.sh F4
  ./compile.sh F7
else
  mkdir -p build
  mkdir -p build/STM32$1

  cd build/STM32$1
  cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=stm32$1.cmake ../..
  ninja
fi



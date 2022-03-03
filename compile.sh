#!/bin/bash

if [ $1 == 'all' ] || [ $1 == 'ALL' ]
then
  ./compile.sh f1
  ./compile.sh f2
  ./compile.sh f4
  ./compile.sh f7
  ./compile.sh g4
else
  mkdir -p build
  mkdir -p build/STM32$1

  cd build/STM32$1
  cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=stm32$1.cmake ../..
  ninja
fi



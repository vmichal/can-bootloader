#!/bin/bash
# Example call: ./compile.sh f1 AMS 8
ARGC=$#

if [ $ARGC -lt 4 ] || [ $ARGC -gt 5   ]
then
  echo "Expected 4 or 5 arguments, received $ARGC."
  echo "Command signature: ./compile.sh mcu-series ECU-name hse-freq_mhz can1-freq-khz"
  echo "Example call ./compile.sh f1 AMS 8 1000"
  exit
fi

#		/////////////////////////////////////////////////////////////////////
#		//                  CUSTOMIZATION POINT
#		/////////////////////////////////////////////////////////////////////
#		List of HSE frequencies:
#		AMS (FSE09, FSE10, DV01) - 8MHz
#		STW (FSE10 assembled into the steering wheel) - 8 MHz
#		STW (FSE10 testing, naked board) - 12 MHz
#		PDL (DV01) - 12 MHz
#		PDL (FSE10, FSE11 EV1) - 8 MHz
#		PDL (FSE11 EV2) - 12 MHz
#		DSH (FSE10, FSE11) - 12 MHz
#		FSB (FSE10) - 8 MHz
#		EBSS (DV01) - 12 MHz
#		SA (DV01) - 12 MHz
#		Disruptor - 12 MHz
#
#
#	  // FSE10 DSH has remaped CAN2!


mcu=$1
ecu=$2
hse=$3
can1=$4
if [ $ARGC == 4 ]
then
  remap_can2=0
elif [ $ARGC == 5 ] && [ $5 = "remap_can2" ]
then
  remap_can2=1
fi
mkdir -p build
mkdir -p build/stm32$mcu

cd build/stm32$mcu
cmake -GNinja -DCMAKE_TOOLCHAIN_FILE=stm32$mcu.cmake -DMCU=$mcu -DECU_NAME=$ecu -DHSE_FREQ=$hse -DCAN1_FREQ=$can1 -DREMAP_CAN2=$remap_can2 ../..
ninja



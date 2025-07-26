#!/bin/python3
import sys, os, collections, subprocess, dataclasses, argparse

from typing import Optional

@dataclasses.dataclass
class Pin:
	port : int
	pin : int
	AF : Optional[int] = None

@dataclasses.dataclass
class CAN:
	peripheral : int
	bitrate_khz : int
	RX : Pin
	TX : Pin

@dataclasses.dataclass
class Config:
	car : str
	ecu : str
	mcu : str
	hse_mhz : int
	can1 : Optional[CAN]
	can2 : Optional[CAN]

def build_configuration(data : Config):
	os.makedirs(f'build/{data.car}_{data.ecu}', exist_ok=True)

	def format_pin(pin):
		# All "old" (pre-CTU24 ECUs) had their CAN peripherals allocated such that the pin alternate function was always 9
		old_design_af = 9
		# This was common to all of (used) STM32F1, F4, F7 in all designs of FSE10, FSE11 and FSE12.
		# With the recent transition to unified MCU STM32G4, we gor rid of half the problem, but another arose:
		# Mapping of individual peripherals to vehicle CAN buses became ECU specific.
		# To lift the metaprogramming burden off of BL software (where it would require horrible macros/templates), this
		# script specifies all of:
		#   - what peripheral is connected to the given vehicle bus
		#   - CAN bus bitrate (for now assuming all are FD-incapable. This will introduce yet another headache later.
		#   - TX and RX pin
		#   - their alternate functions
		return f"'{pin.port}', {pin.pin}, {pin.AF if pin.AF is not None else old_design_af}" if pin is not None else 'None'

	can1_used = data.can1 is not None
	can2_used = data.can2 is not None

	args = [
		'cmake',
		'-GNinja',
		f'-DCMAKE_TOOLCHAIN_FILE=stm32{data.mcu}.cmake',
		f'-DMCU={data.mcu}',
		f'-DECU_NAME={data.ecu}',
		f'-DHSE_FREQ={data.hse_mhz}_MHz',
		f'-DCAN1_used={1 if can1_used else 0}',
		f'-DCAN2_used={1 if can2_used else 0}',
		f'-DCAN1_BITRATE={data.can1.bitrate_khz if can1_used else ""}_kHz',
		f'-DCAN2_BITRATE={data.can2.bitrate_khz if can2_used else ""}_kHz',
		f'-DCAN1_PERIPHERAL={data.can1.peripheral if can1_used else ""}_BASE',
		f'-DCAN2_PERIPHERAL={data.can2.peripheral if can2_used else ""}_BASE',
		f'-DCAN1_RX_pin={format_pin(data.can1.RX) if can1_used else ""}',
		f'-DCAN1_TX_pin={format_pin(data.can1.TX) if can1_used else ""}',
		f'-DCAN2_RX_pin={format_pin(data.can2.RX) if can2_used else ""}',
		f'-DCAN2_TX_pin={format_pin(data.can2.TX) if can2_used else ""}',
		'../..'
	]
	print(' '.join(f'\"{arg}\"' for arg in args))
	subprocess.run(args, cwd=f'./build/{data.car}_{data.ecu}')
	subprocess.run('ninja', cwd=f'./build/{data.car}_{data.ecu}')

def parse_args():
	parser = argparse.ArgumentParser(description="Build eForce Prague Formula bootloader.")

	# Add mutually exclusive group
	parser.add_argument('--all', action='store_true', help="If set, process all units.")
	parser.add_argument('car', nargs='?', help="Provide a car name.")
	parser.add_argument('ecu', nargs='?', help="Provide an ECU name.")

	return parser.parse_args()


if __name__ == '__main__':

	build_data : list[Config] = [
		Config(car='FSE10', ecu='AMS', mcu='f1', hse_mhz=8, can1=CAN(peripheral='CAN1', bitrate_khz=500, RX=Pin(port='A', pin=11), TX=Pin(port='A', pin=12)), can2=CAN(peripheral='CAN2', bitrate_khz=1000, RX=Pin(port='B', pin=12),TX=Pin(port='B',pin= 13))),
		Config(car='FSE10', ecu='DSH', mcu='f1', hse_mhz=8, can1=CAN(peripheral='CAN1', bitrate_khz=500, RX=Pin(port='A', pin=11), TX=Pin(port='A', pin=12)), can2=CAN(peripheral='CAN2', bitrate_khz=1000, RX=Pin(port='B', pin=5), TX=Pin(port='B', pin=6))),

		# FSE12
		Config(car='FSE12', ecu='DSH', mcu='f1', hse_mhz=12, can1=CAN(peripheral='CAN1', bitrate_khz=1000, RX=Pin(port='A', pin=11), TX=Pin(port='A', pin=12)), can2=CAN(peripheral='CAN2', bitrate_khz=1000, RX=Pin(port='B', pin=5), TX=Pin(port='B', pin=6))),
		Config(car='FSE12', ecu='AMS', mcu='f1', hse_mhz=8, can1=CAN(peripheral='CAN1', bitrate_khz=1000, RX=Pin(port='A', pin=11), TX=Pin(port='A', pin=12)), can2=CAN(peripheral='CAN2', bitrate_khz=1000, RX=Pin(port='B', pin=12),TX=Pin(port='B',pin= 13))),
		Config(car='FSE12', ecu='PDL', mcu='f4', hse_mhz=12, can1=CAN(peripheral='CAN1', bitrate_khz=1000, RX=Pin(port='A', pin=11), TX=Pin(port='A', pin=12)), can2=CAN(peripheral='CAN2', bitrate_khz=1000, RX=Pin(port='B', pin=12),TX=Pin(port='B',pin= 13))),
		Config(car='FSE12', ecu='FSB', mcu='f4', hse_mhz=12, can1=CAN(peripheral='CAN1', bitrate_khz=1000, RX=Pin(port='B', pin=8), TX=Pin(port='B', pin=9)), can2=CAN(peripheral='CAN2', bitrate_khz=1000, RX=Pin(port='B', pin=5), TX=Pin(port='B', pin=6))),
		Config(car='FSE12', ecu='STW', mcu='f7', hse_mhz=12, can1=CAN(peripheral='CAN1', bitrate_khz=1000, RX=Pin(port='A', pin=11), TX=Pin(port='A', pin=12)), can2=CAN(peripheral='CAN2', bitrate_khz=1000, RX=Pin(port='B', pin=12),TX=Pin(port='B',pin= 13))),
		Config(car='FSE12', ecu='DRTR', mcu='g4', hse_mhz=12, can1=CAN(peripheral='FDCAN1', bitrate_khz=1000, RX=Pin(port='D', pin=0), TX=Pin(port='D', pin=1)), can2=CAN(peripheral='FDCAN2', bitrate_khz=1000, RX=Pin(port='B', pin=5), TX=Pin(port='B', pin=6))), # corresponds to Disruptor V1_1
		Config(car='FSE12', ecu='DRTF', mcu='g4', hse_mhz=12, can1=CAN(peripheral='FDCAN1', bitrate_khz=1000, RX=Pin(port='D', pin=0), TX=Pin(port='D', pin=1)), can2=CAN(peripheral='FDCAN2', bitrate_khz=1000, RX=Pin(port='B', pin=5), TX=Pin(port='B', pin=6))), # corresponds to Disruptor V1_1
		Config(car='FSE12', ecu='MBOXL', mcu='f4', hse_mhz=12, can1=CAN(peripheral='CAN1', bitrate_khz=1000, RX=Pin(port='A', pin=11), TX=Pin(port='A', pin=12)), can2=None),
		Config(car='FSE12', ecu='MBOXR', mcu='f4', hse_mhz=12, can1=CAN(peripheral='CAN1', bitrate_khz=1000, RX=Pin(port='A', pin=11), TX=Pin(port='A', pin=12)), can2=None),

		# CTU24 (this information is also listed in the thread https://discord.com/channels/1141355470108499979/1250739945052704779)
		Config(car='CTU24', ecu='DRTR', mcu='g4', hse_mhz=12, can1=CAN(peripheral='FDCAN1', bitrate_khz=1000, RX=Pin(port='D', pin=0, AF=9), TX=Pin(port='D', pin=1, AF=9)), can2=CAN(peripheral='FDCAN2', bitrate_khz=1000, RX=Pin(port='B', pin=5, AF=9), TX=Pin(port='B', pin=6, AF=9))), # corresponds to Disruptor V1_2
		Config(car='CTU24', ecu='DRTF', mcu='g4', hse_mhz=12, can1=CAN(peripheral='FDCAN1', bitrate_khz=1000, RX=Pin(port='D', pin=0, AF=9), TX=Pin(port='D', pin=1, AF=9)), can2=CAN(peripheral='FDCAN2', bitrate_khz=1000, RX=Pin(port='B', pin=5, AF=9), TX=Pin(port='B', pin=6, AF=9))), # corresponds to Disruptor V1_2
		Config(car='CTU24', ecu='AMS', mcu='g4', hse_mhz=12, can1=CAN(peripheral='FDCAN1', bitrate_khz=1000, RX=Pin(port='A', pin=11, AF=9), TX=Pin(port='A', pin=12, AF=9)), can2=CAN(peripheral='FDCAN2', bitrate_khz=1000, RX=Pin(port='B', pin=12, AF=9), TX=Pin(port='B', pin=13, AF=9))),
		Config(car='CTU24', ecu='PDL', mcu='g4', hse_mhz=12, can1=CAN(peripheral='FDCAN2', bitrate_khz=1000, RX=Pin(port='B', pin=5, AF=9), TX=Pin(port='B', pin=6, AF=9)), can2=CAN(peripheral='FDCAN3', bitrate_khz=1000, RX=Pin(port='B', pin=3, AF=11), TX=Pin(port='B', pin=4, AF=11))),
		Config(car='CTU24', ecu='VDCU', mcu='g4', hse_mhz=12, can1=CAN(peripheral='FDCAN1', bitrate_khz=1000, RX=Pin(port='A', pin=11, AF=9), TX=Pin(port='A', pin=12, AF=9)), can2=CAN(peripheral='FDCAN2', bitrate_khz=1000, RX=Pin(port='B', pin=12, AF=9), TX=Pin(port='B', pin=13, AF=9))),
		Config(car='CTU24', ecu='FSB', mcu='g4', hse_mhz=12, can1=CAN(peripheral='FDCAN1', bitrate_khz=1000, RX=Pin(port='A', pin=11, AF=9), TX=Pin(port='A', pin=12, AF=9)), can2=CAN(peripheral='FDCAN2', bitrate_khz=1000, RX=Pin(port='B', pin=5, AF=9), TX=Pin(port='B', pin=6, AF=9))),
		Config(car='CTU24', ecu='QUADCONN', mcu='g4', hse_mhz=12, can1=CAN(peripheral='FDCAN3', bitrate_khz=1000, RX=Pin(port='B', pin=3, AF=11), TX=Pin(port='A', pin=15, AF=11)), can2=CAN(peripheral='FDCAN2', bitrate_khz=1000, RX=Pin(port='B', pin=5, AF=9), TX=Pin(port='B', pin=6, AF=9))),
		Config(car='CTU24', ecu='ARB', mcu='g4', hse_mhz=12, can1=None, can2=CAN(peripheral='FDCAN1', bitrate_khz=1000, RX=Pin(port='A', pin=11, AF=9), TX=Pin(port='A', pin=12, AF=9))),
		Config(car='CTU24', ecu='BB', mcu='g4', hse_mhz=12, can1=None, can2=CAN(peripheral='FDCAN1', bitrate_khz=1000, RX=Pin(port='A', pin=11, AF=9), TX=Pin(port='A', pin=12, AF=9))),
		Config(car='CTU24', ecu='TLM', mcu='g4', hse_mhz=12, can1=CAN(peripheral='FDCAN3', bitrate_khz=1000, RX=Pin(port='B', pin=3, AF=11), TX=Pin(port='B', pin=4, AF=11)), can2=CAN(peripheral='FDCAN2', bitrate_khz=1000, RX=Pin(port='B', pin=5, AF=9), TX=Pin(port='B', pin=6, AF=9))),
		Config(car='CTU24', ecu='MBOXL', mcu='g4', hse_mhz=12, can1=None, can2=CAN(peripheral='FDCAN1', bitrate_khz=1000, RX=Pin(port='A', pin=11, AF=9), TX=Pin(port='A', pin=12, AF=9))),
		Config(car='CTU24', ecu='MBOXR', mcu='g4', hse_mhz=12, can1=None, can2=CAN(peripheral='FDCAN1', bitrate_khz=1000, RX=Pin(port='A', pin=11, AF=9), TX=Pin(port='A', pin=12, AF=9))),
		Config(car='CTU24', ecu='STW', mcu='g4', hse_mhz=12, can1=CAN(peripheral='FDCAN1', bitrate_khz=1000, RX=Pin(port='A', pin=11, AF=9), TX=Pin(port='A', pin=12, AF=9)), can2=CAN(peripheral='FDCAN2', bitrate_khz=1000, RX=Pin(port='B', pin=12, AF=9), TX=Pin(port='B', pin=13, AF=9))),

		# TODO fill
		Config(car='CTU24', ecu='DSH', mcu='g4', hse_mhz=12, can1=CAN(peripheral='FDCAN1', bitrate_khz=1000, RX=Pin(port='D', pin=0, AF=9), TX=Pin(port='D', pin=1, AF=9)), can2=CAN(peripheral='FDCAN2', bitrate_khz=1000, RX=Pin(port='B', pin=5, AF=9), TX=Pin(port='B', pin=6, AF=9))),

		# CTU25 (this information is also listed in the thread https://discord.com/channels/1141355470108499979/1250739945052704779)
		Config(car='CTU25', ecu='AMS', mcu='g4', hse_mhz=12, can1=CAN(peripheral='FDCAN1', bitrate_khz=1000, RX=Pin(port='A', pin=11, AF=9), TX=Pin(port='A', pin=12, AF=9)), can2=CAN(peripheral='FDCAN3', bitrate_khz=1000, RX=Pin(port='B', pin=3, AF=11), TX=Pin(port='B', pin=4, AF=11))),
		Config(car='CTU25', ecu='DRTR', mcu='g4', hse_mhz=12, can1=CAN(peripheral='FDCAN1', bitrate_khz=1000, RX=Pin(port='D', pin=0, AF=9), TX=Pin(port='D', pin=1, AF=9)), can2=CAN(peripheral='FDCAN2', bitrate_khz=1000, RX=Pin(port='B', pin=5, AF=9), TX=Pin(port='B', pin=6, AF=9))), # corresponds to Disruptor V1_3
		Config(car='CTU25', ecu='DRTF', mcu='g4', hse_mhz=12, can1=CAN(peripheral='FDCAN1', bitrate_khz=1000, RX=Pin(port='D', pin=0, AF=9), TX=Pin(port='D', pin=1, AF=9)), can2=CAN(peripheral='FDCAN2', bitrate_khz=1000, RX=Pin(port='B', pin=5, AF=9), TX=Pin(port='B', pin=6, AF=9))), # corresponds to Disruptor V1_3
		Config(car='CTU25', ecu='PDL', mcu='g4', hse_mhz=12, can1=CAN(peripheral='FDCAN2', bitrate_khz=1000, RX=Pin(port='B', pin=5, AF=9), TX=Pin(port='B', pin=6, AF=9)), can2=CAN(peripheral='FDCAN3', bitrate_khz=1000, RX=Pin(port='B', pin=3, AF=11), TX=Pin(port='B', pin=4, AF=11))),
		Config(car='CTU25', ecu='VDCU', mcu='g4', hse_mhz=12, can1=CAN(peripheral='FDCAN1', bitrate_khz=1000, RX=Pin(port='A', pin=11, AF=9), TX=Pin(port='A', pin=12, AF=9)), can2=CAN(peripheral='FDCAN2', bitrate_khz=1000, RX=Pin(port='B', pin=12, AF=9), TX=Pin(port='B', pin=13, AF=9))),
		Config(car='CTU25', ecu='FSB', mcu='g4', hse_mhz=12, can1=CAN(peripheral='FDCAN1', bitrate_khz=1000, RX=Pin(port='A', pin=11, AF=9), TX=Pin(port='A', pin=12, AF=9)), can2=CAN(peripheral='FDCAN2', bitrate_khz=1000, RX=Pin(port='B', pin=5, AF=9), TX=Pin(port='B', pin=6, AF=9))),
		Config(car='CTU25', ecu='QUADCONN', mcu='g4', hse_mhz=12, can1=CAN(peripheral='FDCAN3', bitrate_khz=1000, RX=Pin(port='B', pin=3, AF=11), TX=Pin(port='A', pin=15, AF=11)), can2=CAN(peripheral='FDCAN2', bitrate_khz=1000, RX=Pin(port='B', pin=5, AF=9), TX=Pin(port='B', pin=6, AF=9))),
		Config(car='CTU25', ecu='STW', mcu='g4', hse_mhz=12, can1=CAN(peripheral='FDCAN1', bitrate_khz=1000, RX=Pin(port='A', pin=11, AF=9), TX=Pin(port='A', pin=12, AF=9)), can2=CAN(peripheral='FDCAN2', bitrate_khz=1000, RX=Pin(port='B', pin=12, AF=9), TX=Pin(port='B', pin=13, AF=9))),
		Config(car='CTU25', ecu='ARB', mcu='g4', hse_mhz=12, can1=CAN(peripheral='FDCAN2', bitrate_khz=1000, RX=Pin(port='B', pin=12, AF=9), TX=Pin(port='B', pin=13, AF=9)), can2=CAN(peripheral='FDCAN1', bitrate_khz=1000, RX=Pin(port='A', pin=11, AF=9), TX=Pin(port='A', pin=12, AF=9))),

	]

	args = parse_args()

	if args.all:
		for data in reversed(build_data):
			if args.car is None or data.car == args.car:
				build_configuration(data)
	else:

		if None in (args.car, args.ecu):
			print('Missing name of car or ECU.')

		for data in build_data:
			if data.car.lower() == args.car.lower() and data.ecu.lower() == args.ecu.lower():
				build_configuration(data)
				break
		else:
			print(f'Could not find configuration for {args.car} {args.ecu}')




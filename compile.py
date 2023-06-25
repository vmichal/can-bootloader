#!/bin/python3
import sys, os, collections, subprocess

if len(sys.argv) != 3:
	print(f'Expected 2 arguments, got {len(sys.argv)}.\nCall signature: compile.sh formula ECU')
	sys.exit(1)

formula = sys.argv[1]
ECU = sys.argv[2]

Pin = collections.namedtuple('Pin', ['port', 'pin'])

# List of tuples (formula name, ECU name, HSE freq [MHz], CAN1 freq [kbitps], CAN1_RX pin, CAN1_TX pin, CAN2_RX pin, CAN2_TX pin
build_data = [
	('FSE10', 'AMS', 'f1', 8, 1000, ('A', 11), ('A', 12), ('B', 12), ('B', 13)),
	('FSE10', 'DSH', 'f1', 8, 1000, ('A', 11), ('A', 12), ('B', 5), ('B', 6)),

	# FSE12
	('FSE12', 'DSH', 'f1', 12, 1000, ('A', 11), ('A', 12), ('B', 5), ('B', 6)),
	('FSE12', 'AMS', 'f1', 8, 1000, ('A', 11), ('A', 12), ('B', 12), ('B', 13)),
	('FSE12', 'PDL', 'f4', 12, 1000, ('A', 11), ('A', 12), ('B', 12), ('B', 13)),
	('FSE12', 'FSB', 'f4', 12, 1000, ('B', 8), ('B', 9), ('B', 5), ('B', 6)),
	('FSE12', 'STW', 'f7', 12, 1000, ('A', 11), ('A', 12), ('B', 12), ('B', 13)),
	('FSE12', 'DRTF', 'g4', 12, 1000, ('D', 0), ('D', 1), ('B', 12), ('B', 13)),
]

for data in build_data:
	if data[0] == formula and data[1] == ECU:
		break
else:
	print(f'Could not find configuration for {formula} {ECU}')
	sys.exit(1)

_,_,mcu, hse, can1_freq, can1_rx_pin, can1_tx_pin, can2_rx_pin, can2_tx_pin = data

try:
	os.mkdir(f'build')
except FileExistsError:
	pass

try:
	os.mkdir(f'build/{formula}_{ECU}')
except FileExistsError:
	pass

format_pin = lambda x: f'\'{x[0]}\', {x[1]}'

subprocess.run(['cmake', '-GNinja', f'-DCMAKE_TOOLCHAIN_FILE=stm32{mcu}.cmake', f'-DMCU={mcu}', f'-DECU_NAME={ECU}', f'-DHSE_FREQ={hse}', f'-DCAN1_FREQ={can1_freq}', f'-DCAN1_RX_pin={format_pin(can1_rx_pin)}', f'-DCAN1_TX_pin={format_pin(can1_tx_pin)}', f'-DCAN2_RX_pin={format_pin(can2_rx_pin)}', f'-DCAN2_TX_pin={format_pin(can2_tx_pin)}', '../..'], cwd=f'./build/{formula}_{ECU}')
subprocess.run('ninja', cwd=f'./build/{formula}_{ECU}')


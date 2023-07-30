#!/bin/python3
import sys, os, collections, subprocess

def build_configuration(data):
	formula, ECU, mcu, hse, can1_freq, can1_rx_pin, can1_tx_pin, can2_rx_pin, can2_tx_pin = data

	try:
		os.mkdir(f'build')
	except FileExistsError:
		pass

	try:
		os.mkdir(f'build/{formula}_{ECU}')
	except FileExistsError:
		pass

	format_pin = lambda x: f'\'{x[0]}\', {x[1]}' if x is not None else 'None'

	subprocess.run(['cmake', '-GNinja', f'-DCMAKE_TOOLCHAIN_FILE=stm32{mcu}.cmake', f'-DMCU={mcu}', f'-DECU_NAME={ECU}', f'-DHSE_FREQ={hse}', f'-DCAN1_FREQ={can1_freq}', f'-DCAN1_RX_pin={format_pin(can1_rx_pin)}', f'-DCAN1_TX_pin={format_pin(can1_tx_pin)}', f'-DCAN1_used={1 if can1_rx_pin is not None and can1_tx_pin is not None else 0}', f'-DCAN2_RX_pin={format_pin(can2_rx_pin)}', f'-DCAN2_TX_pin={format_pin(can2_tx_pin)}', f'-DCAN2_used={1 if can2_rx_pin is not None and can2_tx_pin is not None else 0}', '../..'], cwd=f'./build/{formula}_{ECU}')
	subprocess.run('ninja', cwd=f'./build/{formula}_{ECU}')


if len(sys.argv) != 3 and (len(sys.argv) != 2 or sys.argv[1].lower() != 'all'):
	print(f'Expected 1 argument "all" or 2 arguments, got {len(sys.argv) - 1}.\nCall signature: "compile.py formula ECU" or "compile.py all"')
	sys.exit(1)

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
	('FSE12', 'DRTR', 'g4', 12, 1000, ('D', 0), ('D', 1), ('B', 5), ('B', 6)), # corresponds to Disruptor V1_1
	('FSE12', 'DRTF', 'g4', 12, 1000, ('D', 0), ('D', 1), ('B', 5), ('B', 6)), # corresponds to Disruptor V1_1
	('FSE12', 'MBOXL', 'f4', 12, 1000, ('A', 11), ('A', 12), None, None),
	('FSE12', 'MBOXR', 'f4', 12, 1000, ('A', 11), ('A', 12), None, None),
]

if len(sys.argv) == 2 and sys.argv[1].lower() == 'all':
	# build all of them!
	for data in reversed(build_data):
		build_configuration(data)
else:
	formula = sys.argv[1]
	ECU = sys.argv[2]

	for data in build_data:
		if data[0] == formula and data[1] == ECU:
			build_configuration(data)
			break
	else:
		print(f'Could not find configuration for {formula} {ECU}')
		sys.exit(1)




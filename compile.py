#!/bin/python3
import sys, os, collections, subprocess, dataclasses

@dataclasses.dataclass
class Pin:
	port : int
	pin : int

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
	can1 : CAN
	can2 : CAN

def build_configuration(data : Config):
	try:
		os.mkdir(f'build')
	except FileExistsError:
		pass

	try:
		os.mkdir(f'build/{data.car}_{data.ecu}',)
	except FileExistsError:
		pass

	format_pin = lambda x: f"'{x.port}', {x.pin}" if x is not None else 'None'

	can1_used = data.can1 is not None
	can2_used = data.can2 is not None

	subprocess.run([
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
		], cwd=f'./build/{data.car}_{data.ecu}')
	subprocess.run('ninja', cwd=f'./build/{data.car}_{data.ecu}')


if len(sys.argv) != 3 and (len(sys.argv) != 2 or sys.argv[1].lower() != 'all'):
	print(f'Expected 1 argument "all" or 2 arguments, got {len(sys.argv) - 1}.\nCall signature: "compile.py formula ECU" or "compile.py all"')
	sys.exit(1)



# List of tuples (formula name, ECU name, HSE freq [MHz], CAN1 freq [kbitps], CAN1_RX pin, CAN1_TX pin, CAN2_RX pin, CAN2_TX pin
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
]

if len(sys.argv) == 2 and sys.argv[1].lower() == 'all':
	# build all of them!
	for data in reversed(build_data):
		build_configuration(data)
else:
	car = sys.argv[1]
	ECU = sys.argv[2]

	for data in build_data:
		if data.car == car and data.ecu == ECU:
			build_configuration(data)
			break
	else:
		print(f'Could not find configuration for {car} {ECU}')
		sys.exit(1)




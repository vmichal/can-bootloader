#!/usr/bin/env python3

import os
import struct
import argparse
import sys
from collections import namedtuple

parser = argparse.ArgumentParser(description='Desktop interface to CAN bootloaders embedded into the electric formula.',\
	formatter_class=argparse.ArgumentDefaultsHelpFormatter)

parser.add_argument('-j', metavar="file", dest='json_files', type=str, action='append', help='add candb json file to parse')
parser.add_argument('-f', dest='feature', type=str, choices=["list", "flash"], default="list", help='choose feature - list bootloader aware units or flash new firmware')
parser.add_argument('-u', dest='unit', type=str, help='Unit to flash.')
parser.add_argument('-x', dest='firmware', type=str, help='Path to hex file for flashing')

parser.add_argument('ocarina',  metavar='ocarina', type=str, help='path to ocarina COM port device (/dev/ocarina, /dev/ttyUSB0,...)')

args = parser.parse_args()

#usage:
# python3.8 flash.py -j $repos/../FSE09-Bootloader.json -f list /dev/ttyS4
# python3.8 flash.py -j $repos/../FSE09-Bootloader.json -f flash -u AMS -x build/AMS.hex /dev/ttyS4
from pycandb.candb import CanDB
import ocarina
import time

#taken from https://stackoverflow.com/questions/2084508/clear-terminal-in-python
def clearscreen(numlines=100):
	"""Clear the console.
numlines is an optional argument used only as a fall-back.
"""
	# Thanks to Steven D'Aprano, http://www.velocityreviews.com/forums
	if os.name == "posix":
		# Unix/Linux/MacOS/BSD/etc
		os.system('clear')
	elif os.name in ("nt", "dos", "ce"):
		# DOS/Windows
		os.system('CLS')
	else:
		# Fallback for other operating systems.
		print('\n' * numlines)


db = CanDB(args.json_files)

oc = ocarina.Ocarina(args.ocarina)
oc.set_silent(False) #send ack frames to estabilish communication between the master and bootloader
oc.set_message_forwarding(True)
oc.query_error_flags() #clear errors
oc.set_bitrate_manual(ocarina.Bitrate(ocarina.BITRATE.KBIT500)) #TODO use autobitrate
oc.query_error_flags()

printtime = time.time()
id_seen = [] #list all of msg ids received during this session

def find_message(owner, name):
	return next(msg for id, msg in db.parsed_messages.items() if msg.owner == owner and msg.description.name == name)

def find_enum(owner, name):
	return next(e for e in db.parsed_enums if e.name == f'{owner}_{name}')

def find_bootloader_beacon(): #returns the obect with message BootloaderBeacon
	return find_message("Bootloader", "Beacon")

def list_bootloader_aware_units():
	beacon = find_bootloader_beacon() #all bootloader aware units transmit this message
	fields = beacon.fields

	try:
		target_index = fields.index(next(field for field in fields if field.description.name == "Target"))
		state_index  = fields.index(next(field for field in fields if field.description.name == "State"))
		flash_index  = fields.index(next(field for field in fields if field.description.name == "FlashSize"))
	except:
		print("ERROR: Given message does not include fields 'Target', 'State', 'FlashSize'", file=sys.stderr)
		return
	
	received = {}
	last_print = time.time() - 1
	last_timestamp = 0
	while True:
		if time.time() - last_print > 1:
			clearscreen()
			last_print = time.time()
			print('Connected bootloader aware units:')
			if len(received) == 0:
				print('None')
				continue
			for unit, data in received.items():
				target_name = fields[target_index].linked_enum.enum[unit].name
				state_name = fields[state_index].linked_enum.enum[data[0]].name
				flash_size = int(data[1])
				connected = time.time() - data[2] < 5
				print(f'{target_name}{" (lost)" if not connected else ""}:\t{flash_size} KiB of Flash\t{state_name}')
			continue

		#receive message and check whether we are interested
		ev = oc.read_event()
		if ev == None or not isinstance(ev, ocarina.CanMsgEvent) or ev.id.value != beacon.identifier:
			continue

		#extract information from message's bits
		db.parseData(beacon.identifier, ev.data, ev.timestamp)
		fields = db.getMsgById(beacon.identifier).fields

		target = fields[target_index] # Identifies the bootloader-aware unit
		state = fields[state_index] 
		flash = fields[flash_index] #Size of available flash

		received[target.value[0]] = (state.value[0], flash.value[0], time.time())


def enumerator_by_name(enumerator, enum):
	return next(val for val in enum.enum if enum.enum[val].name == enumerator)
	
def fold_over_bitor(values):
	result = 0
	for v in values:
		result = (result << 8) | v
	return result

class HexRecord:
	def __init__(self, line):
		assert line[0] == ':'
		self.length = int(line[1:3], 16)
		self.address = int(line[3:7], 16)
		self.type = int(line[7:9], 16)
		self.data = line[9 : 9 + 2 * self.length]

		#colon, len, address, type, data, checksum together shall make the whole record
		assert 1 + 2 + 4 + 2 + len(self.data) + 2 == len(line)

		#check hex record checksum
		assert sum(int(line[i:i+2],16) for i in range(1, len(line)-1,2)) % 256 == 0

		#TODO this assertions should probably be error handling
		if not self.isDataRecord():
			assert self.address == 0

		if self.isEofRecord():
			assert self.length == 0
		elif self.isExtendedLinearAddressRecord() or self.isExtendedSegmentAddressRecord():
			assert self.length == 2
		elif self.isStartLinearAddressRecord():
			assert self.length == 4



	def isDataRecord(self):
		return self.type == 0

	def isEofRecord(self):
		return self.type == 1

	def isExtendedSegmentAddressRecord(self):
		return self.type == 2

	def isExtendedLinearAddressRecord(self):
		return self.type == 4

	def isStartLinearAddressRecord(self):
		return self.type == 5

MemoryBlock = namedtuple('MemoryBlock', ['begin', 'data'])

class Firmware():

	def __init__(self, path):
		with open(path, 'r') as code:
			self.records = [HexRecord(line.rstrip()) for line in code]
		print(f'Input contains {len(self.records)} hex records')

		#Memory map is list of MemoryBlocks (start address + list of bytes)
		self.memory_map, self.entry_point = self.process_hex_records(self.records)
		
		#total size of the firmware in bytes
		self.length = sum(len(block.data) for block in self.memory_map)

		print(f'Firmware memory map ({self.length} bytes in total):')
		for block in self.memory_map:
			print(f'\t[0x{block.begin:08x} - 0x{block.begin + len(block.data) - 1:08x}] {len(block.data):8} B ({len(block.data)/1024:.2f} KiB)')

		self.influenced_pages = self.identify_influenced_pages(self.memory_map)
		print(f'Flashing requires {len(self.influenced_pages)} flash pages to be erased\n')
		
	def identify_influenced_pages(self, memory_map):
		pages = set()
		page_size = 2 * 1024 #TODO make this a customization point
		page_alignment = ~ (page_size - 1)

		for block in memory_map:
			aligned_block_begin = block.begin & page_alignment
			block_end = block.begin + len(block.data)

			for page in range(aligned_block_begin, block_end, page_size):
				pages.add(page)

		return pages

	#takes a list of HexRecords and returns a list of MemoryBlocks
	#returned list models the firmware's memory map
	def process_hex_records(self, records : list):
		assert records.pop().isEofRecord() # EOF record must be the last thing in the file

		memory_map = []

		entry_point = None
		current_block = None
		base_address = 0 # set by extended linear address record


		for index, record in enumerate(records):

			if record.isStartLinearAddressRecord(): #we have address of the entry point
				entry_point = int(record.data, 16) #store it to send it to the bootloader later
				print(f'Recognized entry point address 0x{(entry_point & ~1):08x} ({"Thumb" if entry_point & 1 else "ARM"} mode)')
				continue
			elif record.isExtendedLinearAddressRecord():
				base_address = int(record.data, 16) << 16
				continue
			elif record.isExtendedSegmentAddressRecord():
				base_address = int(record.data, 16) << 4
				continue
			elif record.isEofRecord():
				assert False #There may not be any more eof records
			
			#We have data record
			assert record.isDataRecord()

			# small offset in record + previously set base address
			absolute_address = base_address + record.address

			if current_block is None: #create the first memory block
				current_block = MemoryBlock(absolute_address, [])

			elif absolute_address != current_block.begin + len(current_block.data):
				if len(current_block.data) % 4: #The flash must be programmed 16bits at a time. Assure our data is aligned properly
					print('Memory block not aligned to word boundary. Appending ones (flash untouched) to make it halfword aligned')
					while len(current_block.data) % 4:
						current_block.data.append(0xff)

				memory_map.append(current_block) #append the completed block to our memory map and create a new one
				current_block = MemoryBlock(absolute_address, [])

			for byte in (record.data[i : i + 2] for i in range(0, len(record.data), 2)):
				current_block.data.append(byte) #append all record's bytes to the current memory block

		assert current_block is not None #We sure had some data records, right?
		#TODO entry point recognition my be implemented differently
		assert entry_point is not None #We did find the entry point, right? 
		memory_map.append(current_block)

		return (memory_map,	entry_point)

class FlashMaster():

	transactionMagicString = "Heli"
	transactionMagic = fold_over_bitor([ord(c) for c in transactionMagicString])

	def __init__(self, unit):

		#get references to enumerations
		self.BootTargetEnum = find_enum("Bootloader", "BootTarget")
		self.HandshakeResponseEnum = find_enum("Bootloader", "HandshakeResponse")
		self.RegisterEnum = find_enum("Bootloader", "Register")
		self.StateEnum = find_enum("Bootloader", "State")
		self.WriteResultEnum = find_enum("Bootloader", "WriteResult")


		#get references to messages
		self.EntryReq = find_message("Bootloader", "EntryReq")
		self.EntryAck = find_message("Bootloader", "EntryAck")
		self.ExitReq = find_message("Bootloader", "EntryReq")
		self.ExitAck = find_message("Bootloader", "EntryReq")
		self.Beacon = find_message("Bootloader", "Beacon")
		self.Data = find_message("Bootloader", "Data")
		self.DataAck = find_message("Bootloader", "DataAck")
		self.Handshake = find_message("Bootloader", "Handshake")
		self.HandshakeAck = find_message("Bootloader", "HandshakeAck")
		self.SerialOutput = find_message("Bootloader", "SerialOutput")

		self.unitName = unit
		self.target = enumerator_by_name(self.unitName, self.BootTargetEnum)

		self.print_header()

	def receive_handshake_ack(self, ev, sent):
		fields = self.HandshakeAck.fields

		try:
			register_index = fields.index(next(field for field in fields if field.description.name == "Register"))
			response_index = fields.index(next(field for field in fields if field.description.name == "Response"))
			value_index = fields.index(next(field for field in fields if field.description.name == "Value"))
		except:
			print("ERROR: Message HandshakeAck does not include fields 'Register', 'Response', 'Value'")
			assert False

		assert ev is not None and isinstance(ev, ocarina.CanMsgEvent) and ev.id.value == self.HandshakeAck.identifier

		#extract information from message's bits
		db.parseData(ev.id.value, ev.data, ev.timestamp)
		fields = db.getMsgById(ev.id.value).fields

		#TODO check that this response is to our last transmit
		received = (fields[register_index].value[0], fields[value_index].value[0])

		if received != sent :
			print(f'Received ack to different handshake! Sent {sent}, received {received}')
			return False

		
		enumerator = self.HandshakeResponseEnum.enum[fields[response_index].value[0]].name
		print(enumerator)
		return enumerator == 'OK'

	def receive_data_ack(self, ev, sent):
		fields = self.DataAck.fields

		try:
			address_index = fields.index(next(field for field in fields if field.description.name == "Address"))
			result_index = fields.index(next(field for field in fields if field.description.name == "Result"))
		except:
			print("ERROR: Message DataAck does not include fields 'Address', 'Result'")
			assert False

		assert ev is not None and isinstance(ev, ocarina.CanMsgEvent) and ev.id.value == self.DataAck.identifier

		#extract information from message's bits
		db.parseData(ev.id.value, ev.data, ev.timestamp)
		fields = db.getMsgById(ev.id.value).fields

		#TODO check that this response is to our last transmit
		address = fields[address_index].value[0]
		result = self.WriteResultEnum.enum[fields[result_index].value[0]].name

		if address != sent[0]:
			print(f'Received acknowledge to different data! Written address 0x{sent[0]:08x}, received 0x{address:08x}')
			return False

		return result == 'Ok'

	def receive_exit_ack(self, ev, sent):
		fields = self.ExitAck.fields

		try:
			target_index = fields.index(next(field for field in fields if field.description.name == "Target"))
			confirmed_index = fields.index(next(field for field in fields if field.description.name == "Confirmed"))
		except:
			print("ERROR: Message ExitAck does not include fields 'Target', 'Confirmed'")
			assert False

		assert ev is not None and isinstance(ev, ocarina.CanMsgEvent) and ev.id.value == self.ExitAck.identifier

		#extract information from message's bits
		db.parseData(ev.id.value, ev.data, ev.timestamp)
		fields = db.getMsgById(ev.id.value).fields

		#TODO check that this response is to our last transmit
		target = fields[target_index].value[0]
		confirmed = fields[confirmed_index].value[0]

		if target != sent[0]:
			enum = self.BootTargetEnum.enum
			print(f'Received ExitAck from different unit! Targeted {enum[target]} but {enum[sent[0]]} responded.')
			return False

		print("Confirmed" if confirmed else "Refused")
		return confirmed

	def receive_entry_ack(self, ev, sent):
		fields = self.EntryAck.fields

		try:
			target_index = fields.index(next(field for field in fields if field.description.name == "Target"))
			confirmed_index = fields.index(next(field for field in fields if field.description.name == "Confirmed"))
		except:
			print("ERROR: Message EntryAck does not include fields 'Target', 'Confirmed'")
			assert False

		assert ev is not None and isinstance(ev, ocarina.CanMsgEvent) and ev.id.value == self.EntryAck.identifier

		#extract information from message's bits
		db.parseData(ev.id.value, ev.data, ev.timestamp)
		fields = db.getMsgById(ev.id.value).fields

		#TODO check that this response is to our last transmit
		target = fields[target_index].value[0]
		confirmed = fields[confirmed_index].value[0]

		if target != sent[0]:
			enum = self.BootTargetEnum.enum
			print(f'Received EntryAck from different unit! Targeted {enum[target]} but {enum[sent[0]]} responded.')
			return False

		print("Confirmed" if confirmed else "Refused")
		return confirmed

	#returns pair (target, state)
	def receive_beacon(self, ev):
		fields = self.Beacon.fields

		try:
			target_index = fields.index(next(field for field in fields if field.description.name == "Target"))
			state_index = fields.index(next(field for field in fields if field.description.name == "State"))
		except:
			print("ERROR: Message Beacon does not include fields 'Target', 'State'")
			assert False

		assert ev is not None and isinstance(ev, ocarina.CanMsgEvent) and ev.id.value == self.Beacon.identifier

		#extract information from message's bits
		db.parseData(ev.id.value, ev.data, ev.timestamp)
		fields = db.getMsgById(ev.id.value).fields

		#TODO check that this response is to our last transmit
		target = fields[target_index].value[0]
		state = fields[state_index].value[0]

		return (target, state)

	#print piece of bootloader's standard output to the output file
	def receive_serial_output(self, ev, output_file = sys.stderr):
		fields = self.SerialOutput.fields

		try:
			seq_index = fields.index(next(field for field in fields if field.description.name == "SEQ"))
			truncated_index = fields.index(next(field for field in fields if field.description.name == "Truncated"))
			completed_index = fields.index(next(field for field in fields if field.description.name == "Completed"))
			payload_index = fields.index(next(field for field in fields if field.description.name == "Payload"))
		except:
			print("ERROR: Given message does not include fields 'SEQ', 'Truncated', 'Completed', 'Payload'")
			assert False

		assert ev is not None and isinstance(ev, ocarina.CanMsgEvent) and ev.id.value == self.SerialOutput.identifier

		#extract information from message's bits
		db.parseData(self.SerialOutput.identifier, ev.data, ev.timestamp)
		fields = db.getMsgById(self.SerialOutput.identifier).fields

		seq = fields[seq_index] # sequence counter, increasing number between 0 and (typically) 15
		truncated = fields[truncated_index] 
		completed = fields[completed_index] #Board sent all enqued data
		payload = fields[payload_index] # 7 characters of output

		output_file.write("".join(map(chr, map(int, filter(lambda c: c != 0, payload.value)))))
		if completed.value[0]:
			output_file.flush()

	def wait_for_message(self, expected_id, sent):
		while True:
			#receive message and check whether we are interested
			ev = oc.read_event()
			if ev == None or not isinstance(ev, ocarina.CanMsgEvent):
				continue

			if ev.id.value == self.HandshakeAck.identifier:
				ret = self.receive_handshake_ack(ev, sent)
			elif ev.id.value == self.DataAck.identifier:
				ret = self.receive_data_ack(ev, sent)
			elif ev.id.value == self.SerialOutput.identifier:
				ret = self.receive_serial_output(ev)
			elif ev.id.value == self.ExitAck.identifier:
				ret = self.receive_exit_ack(ev, sent)
			elif ev.id.value == self.EntryAck.identifier:
				ret = self.receive_entry_ack(ev, sent)

			if ev.id.value == expected_id:
				return ret

	#Customization point for other low level communication drivers
	def send_message(self, id, data):
		apostrophe = "'"

		#print(f"Note: Sending message {hex(id)} with data 0x{apostrophe.join('{:02x}'.format(val) for val in data)}")
		oc.send_message_std(id, data)

	def send_handshake(self, reg, value):
		buffer = [0] * 5
		self.Handshake.assemble([reg, value],buffer)

		attempts = 0
		self.send_message(self.Handshake.identifier, buffer)
		while attempts < 5 and not self.wait_for_message(self.HandshakeAck.identifier, (reg, value)):
			self.send_message(self.Handshake.identifier, buffer)
			attempts = attempts + 1

		if attempts == 5:
			print('Could not proceed.')
			sys.exit(0)

	def send_data(self, address, data):
		assert len(data) == 4 # we support only word writes now
		buffer = [0] * 8
		word = int(''.join(data[::-1]), 16)
		shifted_address = address >> 2
		self.Data.assemble([shifted_address, False, word], buffer)

		attempts = 0
		self.send_message(self.Data.identifier, buffer)
		while attempts < 5 and not self.wait_for_message(self.DataAck.identifier, (shifted_address, word)):
			self.send_message(self.Data.identifier, buffer)
			attempts = attempts + 1
			
		if attempts == 5:
			print('Could not proceed.')
			sys.exit(0)
		

	def send_transaction_magic(self):
		self.send_handshake(enumerator_by_name("TransactionMagic", self.RegisterEnum), FlashMaster.transactionMagic)

	def request_bootloader_exit(self, target):
		buffer = [0]
		self.ExitReq.assemble([target], buffer)

		attempts = 0
		self.send_message(self.ExitReq.identifier, buffer)
		while attempts < 5 and not self.wait_for_message(self.ExitAck.identifier, [target]):
			self.send_message(self.ExitReq.identifier, buffer)
			attempts = attempts + 1

		if attempts == 5:
			print('Could not proceed.')
			sys.exit(0)

	def request_bootloader_entry(self, target):
		buffer = [0]
		self.EntryReq.assemble([target], buffer)

		attempts = 0
		self.send_message(self.EntryReq.identifier, buffer)
		while attempts < 5 and not self.wait_for_message(self.EntryAck.identifier, [target]):
			self.send_message(self.EntryReq.identifier, buffer)
			attempts = attempts + 1

		if attempts == 5:
			print('Could not proceed.')
			sys.exit(0)

	def get_target_state(self, target):
		while True:
			#receive message and check whether we are interested
			ev = oc.read_event()
			if ev == None or not isinstance(ev, ocarina.CanMsgEvent):
				continue

			if ev.id.value == self.Beacon.identifier:
				unit, state = self.receive_beacon(ev)
				print(f'{self.BootTargetEnum.enum[unit].name}\t{self.StateEnum.enum[state].name}')
				if unit == target:
					return self.StateEnum.enum[state].name


	def await_bootloader_ready(self, target):
		#TODO merge this with get_target_state
		while True:
			#receive message and check whether we are interested
			ev = oc.read_event()
			if ev == None or not isinstance(ev, ocarina.CanMsgEvent):
				continue

			if ev.id.value == self.Beacon.identifier:
				unit, state = self.receive_beacon(ev)
				if unit == target and self.StateEnum.enum[state].name == 'Ready':
					print('Ok')
					return

	def print_header(self):
		print('Desktop interface to CAN Bootloader')
		print('Copyright (c) Vojtech Michal, eForce FEE Prague Formula 2020\n')

		targets = [self.BootTargetEnum.enum[val].name for val in self.BootTargetEnum.enum]
		if len(targets) == 1:
			print(f"Targetable unit is {targets[0]}")
		else:
			print(f"Targetable units are {', '.join(targets)}")
		print(f"Starting flash process for {self.unitName} (target id {self.target})\n")

	def parseFirmware(self, firmwarePath):
		print(f'Parsing hex file {firmwarePath}')
		self.firmware = Firmware(firmwarePath)

	def flash(self):
		print('Searching bootloader aware units present on the bus:')
		state = self.get_target_state(self.target)
		print("Target's presence on the CAN bus confirmed.")

		if state == 'FirmwareActive':
			print(f'Sending request to {self.BootTargetEnum.enum[self.target].name} to enter the bootloader ... ', end = '')
			self.request_bootloader_entry(self.target)
			print(f'Waiting for {self.BootTargetEnum.enum[self.target].name} bootloader to respond ...  ', end='')
			self.await_bootloader_ready(self.target)
		elif state == 'Ready':
			print('Target already in bootloader.')
		else:
			print('Cannot interfere with other ongoing flash transaction.')
			sys.exit(1)

		print('Flash transaction starts\n')
		
		print('Sending initial transaction magic ... ', end = '')
		self.send_transaction_magic()
		
		print('Sending the size of new firmware ... ', end = '')
		self.send_handshake(enumerator_by_name("FirmwareSize", self.RegisterEnum), self.firmware.length)
		
		print('Sending the number of pages to erase ... ', end = '')
		self.send_handshake(enumerator_by_name('NumPagesToErase', self.RegisterEnum), len(self.firmware.influenced_pages))
		
		print('Sending page addresses')
		for index, page in enumerate(self.firmware.influenced_pages):
			print(f'Erasing page {index} @ 0x{page:08x} ... ', end = '')
			self.send_handshake(enumerator_by_name('PageToErase', self.RegisterEnum), page)
		
		#TODO send one more magic here

		print('Sending entry point address ... ', end = '')
		self.send_handshake(enumerator_by_name('EntryPoint', self.RegisterEnum), self.firmware.entry_point)

		print('Sending interrupt vector ... ', end = '')
		#TODO make sure this is really the interrupt vector
		self.send_handshake(enumerator_by_name('InterruptVector', self.RegisterEnum), self.firmware.memory_map[0].begin)

		print('Sending second transaction magic ... ', end = '')
		self.send_transaction_magic()

		checksum = 0
		for block in self.firmware.memory_map:
			assert len(block.data) % 4 == 0 #all messages will carry whole word

			for offset in range(0, len(block.data), 4):
				absolute_address = block.begin + offset
				data = block.data[offset : offset + 4]
				data_as_word = int("".join(data[::-1]), 16)
				print(f'Programming 0x{absolute_address:08x} = 0x{data_as_word:08x} ... ', end = '')
				self.send_data(absolute_address, data)
				print('Ok')
				checksum += (data_as_word >> 16) + (data_as_word & 0xffff)
		
		print(f'Firmware checksum = {checksum:08x}')

		print('Sending checksum ... ', end = '')
		self.send_handshake(enumerator_by_name('Checksum', self.RegisterEnum), checksum)

		print('Sending last transaction magic ... ', end = '')
		self.send_transaction_magic()

		print('Flashed successfully')
		print('Entering the firmware ... ')
		self.request_bootloader_exit(self.target)

		#TODO make the bootloader exit to firmware

		


clearscreen()
if args.feature == 'list':
	list_bootloader_aware_units();

elif args.feature == 'flash':
	#TODO add error checking if there is no -u or -f
	master = FlashMaster(args.unit)
	master.parseFirmware(args.firmware)
	master.flash()

else:
	assert False

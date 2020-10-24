#!/usr/bin/env python3

import os
import struct
import argparse
import sys
import threading
import itertools
import queue
from collections import namedtuple

parser = argparse.ArgumentParser(description="=========== CAN bootloader ============\nDesktop interface to CAN bootloaders embedded into the electric formula.\nWritten by Vojtech Michal, (c) eForce FEE Prague Formula 2020",\
	formatter_class=argparse.RawTextHelpFormatter)

parser.add_argument('-j', metavar="file", dest='json_files', type=str, action='append', help='add candb json file to parse')
parser.add_argument('-f', dest='feature', type=str, choices=["list", "flash"], default="list", help='choose feature - list bootloader aware units or flash new firmware')
parser.add_argument('-u', dest='unit', type=str, help='Unit to flash.')
parser.add_argument('-x', dest='firmware', type=str, help='Path to hex file for flashing')
parser.add_argument('-t', dest='terminal', type=str, help='Path to second terminal for pretty bootloader listing')

parser.add_argument('ocarina',  metavar='ocarina', type=str, help='path to ocarina COM port device (/dev/ocarina, /dev/ttyUSB0,...)')

args = parser.parse_args()

#usage:
# python3.8 flash.py -j $repos/../FSE09-Bootloader.json -f list /dev/ttyS4
# python3.8 flash.py -j $repos/../FSE09-Bootloader.json -f flash -u AMS -x build/AMS.hex /dev/ttyS4
from pycandb.candb import CanDB
import ocarina_sw.api.ocarina as ocarina
import time

#taken from https://stackoverflow.com/questions/2084508/clear-terminal-in-python
def clearscreen(terminal, numlines=100):
	"""Clear the console.
numlines is an optional argument used only as a fall-back.
"""
	# Thanks to Steven D'Aprano, http://www.velocityreviews.com/forums
	if os.name == "posix":
		# Unix/Linux/MacOS/BSD/etc
		os.system(f'clear > {terminal}')
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
oc.set_bitrate_auto()

def find_message(owner, name):
	try:
		return next(msg for id, msg in db.parsed_messages.items() if msg.owner == owner and msg.description.name == name)
	except:
		raise Exception(f"Message {owner}::{name} does not exist!")

def find_enum(owner, name):
	try:
		return next(e for e in db.parsed_enums if e.name == f'{owner}_{name}')
	except:
		raise Exception(f"Enum {owner}::{name} does not exist!")

TargetData = namedtuple('TargetData', ['state', 'flash_size', 'last_response', 'entry_reason'])
TargetSoftwareBuild = namedtuple('TargetSoftwareBuild', ['CommitSHA', 'DirtyRepo'])
ApplicationData = namedtuple('ApplicationData', ['BLpending', 'last_response'])

class BootloaderListing:
	def __init__(self, oc, canDB, terminal):
		self.terminal_name = terminal
		self.terminal = open(terminal, 'w') if terminal is not None else None

		self.oc = oc
		self.candb = canDB
		self.beacon = find_message("Bootloader", "Beacon") #all active bootloaders transmit this message
		self.pingResponse = find_message("Bootloader", "PingResponse") #whereas units with active firmware transmit this
		self.ping = find_message("Bootloader", "Ping")
		self.SoftwareBuild = find_message("Bootloader", "SoftwareBuild")
	
		try:
			self.beacon["Target"]
			self.beacon["State"]
			self.beacon["FlashSize"]
			self.beacon["EntryReason"]
		except:
			raise Exception("ERROR: Message 'Beacon' does not include fields 'Target', 'State', 'FlashSize'")
		try:
			self.pingResponse["Target"]
			self.pingResponse["BootloaderPending"]
		except:
			raise Exception("ERROR: Message 'PingResponse' does not include fields 'Target', 'BootloaderPending'")

		try:
			self.SoftwareBuild["CommitSHA"]
			self.SoftwareBuild["DirtyRepo"]
			self.SoftwareBuild["Target"]
		except:
			raise Exception("ERROR: Message 'SoftwareBuild' does not include fields 'Target', 'CommitSHA', 'DirtyRepo'")

		self.targetable_units = self.beacon["Target"].linked_enum.enum
		self.active_bootloaders  = { }
		self.bootloader_versions = { }
		self.aware_applications  = { unit : ApplicationData(None, None) for unit in self.targetable_units }

		self.exit = False
		self.receiving_acks = False

		if self.terminal is not None:
			self.printThread = threading.Thread(target = BootloaderListing._do_print, args = (self,), daemon=True)
			self.printThread.start()
		self.pingThread = threading.Thread(target = BootloaderListing._do_ping_bootloader_aware_units, args = (self,), daemon=True)
		self.pingThread.start()

	def _do_print(self):
		while not self.exit:
			assert self.terminal is not None
			time.sleep(1)
			clearscreen(self.terminal_name)

			if not self.receiving_acks:
				print('WARNING: Ocarina is not receving acknowledge frames!\n', file = self.terminal)

			print('State of bootloader aware units:\n', file = self.terminal)
			if len(self.aware_applications) == 0 and len(self.active_bootloaders) == 0:
				print('None', file = self.terminal)
				continue

			targets = []
			states = []
			flash_sizes = []
			entry_reasons = []
			commits = []
			last_response = []
	
			#copy data to separate lists
			for unit, data in self.active_bootloaders.items():
				commits.append('N/A')
				if unit in self.bootloader_versions:
					commit = self.bootloader_versions[unit]

					if commit.CommitSHA is not None:
						commits[-1] = f'0x{commit.CommitSHA:08x}{"(dirty)" if commit.DirtyRepo else ""}'

				targets.append(self.beacon["Target"].linked_enum.enum[unit].name)
				states.append(self.beacon["State"].linked_enum.enum[data.state].name)
				entry_reasons.append(self.beacon["EntryReason"].linked_enum.enum[data.entry_reason].name)
				flash_sizes.append(str(data.flash_size))
				last_response.append("yep" if time.time() - data.last_response < 2.0 else "connection lost")
	
			for unit, data in self.aware_applications.items():
				targets.append(self.beacon["Target"].linked_enum.enum[unit].name)
				flash_sizes.append('')
				commits.append('')
				entry_reasons.append('')

				if data.last_response is not None:
					last_response.append("yep" if time.time() - data.last_response < 2.0 else "connection lost")
					state = "FirmwareRunning" if not data.BLpending else "BLpending"
				else:
					last_response.append("Never seen")
					state = 'Unknown'

				states.append(state)

				
			#find longest strings in each list
			space_width = 6
			length_target    = space_width + max(max(map(lambda s:len(s), targets))         , len("Target"))
			length_state     = space_width + max(max(map(lambda s:len(s), states))          , len("State"))
			length_reason    = space_width + max(max(map(lambda s:len(s), entry_reasons))   , len("Activation reason"))
			length_flash     = space_width + len("Flash [KiB]")
			length_commit    = space_width + max(max(map(lambda s:len(s), commits))         , len("Bootloader build"))
			length_connected = len("connection lost")
	
			print(f'{"Target":{length_target}}{"State":{length_state}}{"Flash [KiB]":{length_flash}}{"Activation reason":{length_reason}}{"Bootloader build":{length_commit}}{"Responding?":{length_connected}}', file = self.terminal)
			print('-' * (length_target + length_state + length_reason + length_flash + length_connected + length_commit), file = self.terminal)
			for target, state, reason, flash_size, commit, response in zip(targets, states, entry_reasons, flash_sizes, commits, last_response):
				print(f'{target:{length_target}}{state:{length_state}}{flash_size:{length_flash}}{reason:{length_reason}}{commit:{length_commit}}{response:{length_connected}}', file = self.terminal)
		
	def _do_ping_bootloader_aware_units(self):
		#cycle through all possible boot targets and ping them
		while not self.exit:
			targets = list(filter(lambda t: t not in self.active_bootloaders, self.targetable_units)) #target only those units that are not in bootloader

			for unit in itertools.cycle(targets):
				buffer = [0] * 1
				self.ping.assemble([unit, False], buffer)
	
				if self.receiving_acks: #there is some other node on the bus
					self.oc.send_message_std(self.ping.identifier, buffer)
				time.sleep(1.0 / len(targets))

	def process_event(self, ev):
		if isinstance(ev, ocarina.CANErrorEvent): #there is some error on CAN
			if ev.eType.value == ocarina.CANERROR.ACKNOWLEDGMENT:
				self.receiving_acks = False
			else:
				print(f'Other error {ev}', file = sys.stderr)
			return

		elif isinstance(ev, ocarina.HeartbeatEvent) or isinstance(ev, ocarina.ErrorFlagsEvent):
			return#ignore HeartbeatEvent
	
		elif not isinstance(ev, ocarina.CanMsgEvent): #ignore other events
			print(f'other event {ev}', file= sys.stderr)
			return
	
		if not self.receiving_acks:
			self.receiving_acks = True  
			self.oc.query_error_flags()

		if ev.id.value == self.beacon.identifier: #we have received a bootloader beacon
			#extract information from message's bits
			self.candb.parseData(ev.id.value, ev.data, ev.timestamp)

			#append its data to the list
			target = self.beacon["Target"].value[0]
			state = self.beacon["State"].value[0]
			flash_size = int(self.beacon["FlashSize"].value[0])
			entry_reason = self.beacon["EntryReason"].value[0]


			self.active_bootloaders[target] = TargetData(state, flash_size, time.time(), entry_reason)
			if target in self.aware_applications:
				del self.aware_applications[target] #target has entered bootloader -> remove it from list of aware applications
	
		elif ev.id.value == self.pingResponse.identifier: #some bootloader aware unit responded
			#extract information from message's bits
			self.candb.parseData(ev.id.value, ev.data, ev.timestamp)
			state = "FirmwareRunning" if not self.pingResponse["BootloaderPending"].value[0] else "BLpending"
			target = self.pingResponse["Target"].value[0]

			self.aware_applications[target] = TargetData(state, None, time.time(), None)
			if target in self.active_bootloaders: #remove the target from list of active bootloaders
				del self.active_bootloaders[target]


		elif ev.id.value == self.SoftwareBuild.identifier:
			#extract information from message's bits
			self.candb.parseData(ev.id.value, ev.data, ev.timestamp)

			target = self.SoftwareBuild["Target"].value[0]
			sha = int(self.SoftwareBuild["CommitSHA"].value[0])
			dirty = bool(self.SoftwareBuild["DirtyRepo"].value[0])

			self.bootloader_versions[target] = TargetSoftwareBuild(sha, dirty)

	def terminate(self):
		self.exit = True

		pingThread.join()
		if self.terminal is not None:
			printThread.join()

def enumerator_by_name(enumerator, enum):
	try:
		return next(val for val, elem in enum.enum.items() if elem.name == enumerator)
	except:
		raise Exception(f"Enumerator {enum}::{enumerator} does not exist!")

class HexRecord:
	def __init__(self, line):
		if line[0] != ':':
			raise Exception(f'Invalid hex record {line} (missing starting colon).')
		self.length = int(line[1:3], 16)
		self.address = int(line[3:7], 16)
		self.type = int(line[7:9], 16)
		self.data = line[9 : 9 + 2 * self.length]

		#colon, len, address, type, data, checksum together shall make the whole record
		assert 1 + 2 + 4 + 2 + len(self.data) + 2 == len(line)

		#check hex record checksum
		checksum = sum(int(line[i:i+2],16) for i in range(1, len(line), 2)) % 256
		if checksum != 0:
			raise Exception(f'Checksum {checksum} of hex record {line} does not match!')

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
		print(f'Input consists of {len(self.records)} hex records.')

		#Memory map is list of MemoryBlocks (start address + list of bytes)
		self.logical_memory_map, self.entry_point = self.process_hex_records(self.records)
		
		#total size of the firmware in bytes
		self.length = sum(len(block.data) for block in self.logical_memory_map)

		print(f'Firmware logical memory map ({self.length} bytes in total):')
		for block in self.logical_memory_map:
			print(f'\t[0x{block.begin:08x} - 0x{block.begin + len(block.data) - 1:08x}] ... {len(block.data):8} B ({len(block.data)/1024:.2f} KiB)')

	def identify_influenced_pages(self, logical_memory_map):
		pages = set()
		page_size = 2 * 1024 #TODO make this a customization point
		page_alignment = ~ (page_size - 1)

		for block in logical_memory_map:
			aligned_block_begin = block.begin & page_alignment
			block_end = block.begin + len(block.data)

			for page in range(aligned_block_begin, block_end, page_size):
				pages.add(page)

		return pages

	#takes a list of HexRecords and returns a list of MemoryBlocks
	#returned list models the firmware's memory map
	def process_hex_records(self, records : list):
		if not records.pop().isEofRecord(): # EOF record must be the last thing in the file
			raise Exception('Input file was not terminated by OEF hex record!')

		logical_memory_map = []

		entry_point = None
		current_block = None
		base_address = 0 # set by extended linear address record


		for record in records:

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
				raise Exception('There shall not be multiple eof records in a valid input file.')
			
			#We have data record
			assert record.isDataRecord()

			# small offset in record + previously set base address
			absolute_address = base_address + record.address

			if current_block is None: #create the first memory block
				current_block = MemoryBlock(absolute_address, [])

			elif absolute_address != current_block.begin + len(current_block.data):
				#we are starting a new memory block

				#TODO make this '2' a customization point (send flash memory alignment in Beacon)
				if len(current_block.data) % 2: #The flash must be programmed 16bits at a time. Assure our data is aligned properly
					print(f'Memory block {current_block} not aligned to halfword boundary. Appending ones (flash untouched) to make it halfword aligned.')
					while len(current_block.data) % 2:
						current_block.data.append(0xff)

				logical_memory_map.append(current_block) #append the completed block to our memory map and create a new one
				current_block = MemoryBlock(absolute_address, [])

			for byte in (record.data[i : i + 2] for i in range(0, len(record.data), 2)):
				current_block.data.append(int(byte,16)) #append all record's bytes to the current memory block

		assert current_block is not None #We sure had some data records, right?
		#TODO entry point recognition my be implemented differently
		assert entry_point is not None #We did find the entry point, right? 
		logical_memory_map.append(current_block)

		return (logical_memory_map,	entry_point)

class FlashMaster():

	transactionMagicString = "Heli"
	transactionMagic = sum(ord(char) << 8*index for index, char in enumerate(transactionMagicString))

	def __init__(self, unit, other_terminal):

		#get references to enumerations
		self.BootTargetEnum = find_enum("Bootloader", "BootTarget")
		self.HandshakeResponseEnum = find_enum("Bootloader", "HandshakeResponse")
		self.RegisterEnum = find_enum("Bootloader", "Register")
		self.StateEnum = find_enum("Bootloader", "State")
		self.WriteResultEnum = find_enum("Bootloader", "WriteResult")
		self.CommandEnum = find_enum('Bootloader', 'Command')
		self.EntryReasonEnum = find_enum('Bootloader', 'EntryReason')


		#get references to messages
		self.Ping = find_message("Bootloader", "Ping")
		self.PingResponse = find_message("Bootloader", "PingResponse")
		self.ExitReq = find_message("Bootloader", "ExitReq")
		self.ExitAck = find_message("Bootloader", "ExitAck")
		self.Beacon = find_message("Bootloader", "Beacon")
		self.Data = find_message("Bootloader", "Data")
		self.DataAck = find_message("Bootloader", "DataAck")
		self.Handshake = find_message("Bootloader", "Handshake")
		self.HandshakeAck = find_message("Bootloader", "HandshakeAck")
		self.CommunicationYield = find_message('Bootloader', 'CommunicationYield')
		self.SoftwareBuild = find_message('Bootloader', 'SoftwareBuild')

		self.targetName = unit
		self.target = enumerator_by_name(self.targetName, self.BootTargetEnum)
		self.exit = False
		self.isBusMaster = True

		self.db = CanDB(args.json_files) #create our own version of CanDB

		self.listing = BootloaderListing(oc, db, other_terminal)
		self.ocarinaReadingThread = threading.Thread(target = FlashMaster.eventReadingThread, args=(self,), daemon = True)

		self.message_queue = queue.Queue()

		self.print_header()

	def eventReadingThread(self):
		while not self.exit:
			ev = oc.read_event(blocking = True)
			assert ev is not None
			self.listing.process_event(ev)
			self.message_queue.put(ev)

			if isinstance(ev, ocarina.CanMsgEvent) and ev.id.value == self.Handshake.identifier: #test for abortion...
				self.db.parseData(ev.id.value, ev.data, ev.timestamp)
				if self.Handshake['Command'].value[0] == enumerator_by_name('AbortTransaction', self.CommandEnum):
					print('Bootloader aborted transaction!')
					sys.exit(1) #todo this deserves a bit nicer handling

	def receive_generic_response(self, id, sent : tuple, must_match : list, wanted : list):
		message = self.db.getMsgById(id)

		for field in must_match:
			if sent._asdict()[field] != message[field].value[0]:
				print(f'Received unrelated acknowledge!')
				return False

		return [message[field] for field in wanted]

	def wait_for_response(self, expected_id, sent):
		while True:
			#receive message and check whether we are interested
			ev = self.message_queue.get()
			assert ev is not None
			if not isinstance(ev, ocarina.CanMsgEvent):
				continue

			#extract information from message's bits
			self.db.parseData(ev.id.value, ev.data, ev.timestamp)

			if ev.id.value in (self.SoftwareBuild.identifier, self.Beacon.identifier):
				continue #these messages are not part of protocol - ignore them

			if ev.id.value == self.HandshakeAck.identifier:
				ret = self.receive_generic_response(self.HandshakeAck.identifier, sent, ['Register', 'Value'], ['Response'])
			elif ev.id.value == self.Handshake.identifier:
				assert False #shall not be received here
			elif ev.id.value == self.CommunicationYield.identifier:
				assert False #shall not be received here
			elif ev.id.value == self.Data.identifier:
				assert False #shall not be received yet
			elif ev.id.value == self.DataAck.identifier:
				ret = self.receive_generic_response(self.DataAck.identifier, sent, ['Address'], ['Result'])
			elif ev.id.value == self.ExitAck.identifier:
				ret = self.receive_generic_response(self.ExitAck.identifier, sent, ['Target'], ['Confirmed'])
			elif ev.id.value == self.PingResponse.identifier:
				ret = self.receive_generic_response(self.PingResponse.identifier, sent, ['Target'], ['Bootloader'])

			print(f'Done {ev.id.value}, waiting for {expected_id}')
			if ev.id.value == expected_id:
				return ret

	def send_generic_message_and_await_response(self, id : int, data : list, responseID : int, await_response = True):
		message = db.getMsgById(id)
		buffer = [0] * message.length

		sent = message.namedtuple(*data)
		message.assemble(data, buffer)
		oc.send_message_std(id, buffer)


		if await_response:
			return self.wait_for_response(responseID, sent)

	def send_handshake(self, reg, command, value):
		return self.send_generic_message_and_await_response(self.Handshake.identifier, [reg, command, value], self.HandshakeAck.identifier)

	def send_data(self, address, data : list):
		assert len(data) == 4 # we support only word writes now
		word = sum(byte << 8 * index for index, byte in enumerate(byte))

		return self.send_generic_message_and_await_response(self.Data.identifier, [address, word], self.DataAck.identifier)

	def send_handshake_ack(self, reg, response, value):
		return self.send_generic_message_and_await_response(self.HandshakeAck.identifier, [reg, response, value], None, False)

	def yield_communication():
		self.isBusMaster = False
		return self.send_generic_message_and_await_response(self.CommunicationYield.identifier, [self.target], None, False)

	def send_transaction_magic(self):
		return self.send_handshake(enumerator_by_name("TransactionMagic", self.RegisterEnum), enumerator_by_name("None", self.CommandEnum), FlashMaster.transactionMagic)

	def request_bootloader_exit(self, target, force):
		return self.send_generic_message_and_await_response(self.ExitReq.identifier, [target, force], self.ExitAck.identifier)

	def request_bootloader_entry(self, target):
		return self.send_generic_message_and_await_response(self.Ping.identifier, [target, True], self.PingResponse.identifier)

	def print_header(self):
		print('Desktop interface to CAN Bootloader')
		print('Written by Vojtech Michal, (c) eForce FEE Prague Formula 2020\n')

		targets = [elem.name for val, elem in self.BootTargetEnum.enum.items()]
		if len(targets) == 1:
			print(f"The only targetable unit is {targets[0]}")
		else:
			print(f"Targetable units are {', '.join(targets)}")
		print(f"Initiating flash process for {self.targetName} (target id {self.target})\n")

	def parseFirmware(self, firmwarePath):
		print(f'Parsing hex file {firmwarePath}')
		self.firmware = Firmware(firmwarePath)

	def flash(self):
		self.ocarinaReadingThread.start()

		print('Searching bootloader aware units present on the bus...')

		is_bootloader_active = lambda target: target in self.listing.active_bootloaders
		is_application_active = lambda target: target in self.listing.aware_applications and self.listing.aware_applications[target].last_response is not None

		while not is_bootloader_active(self.target) and not is_application_active(self.target):
			time.sleep(0.1)
		
		print("Target's presence on the CAN bus confirmed.")

		if is_application_active(self.target):
			print(f'Sending request to {self.target_name} to reset into bootloader ... ', end = '')
			self.request_bootloader_entry(self.target)
			print(f'Waiting for {self.target_name} bootloader to respond ...  ', end='')
			while not is_bootloader_active(self.target):
				time.sleep(0.5)

		elif is_bootloader_active(self.target):
			print('Target is already in bootloader.')
			#TODO use --force cli argument
			ready_state = enumerator_by_name('Ready', self.StateEnum)
			if self.listing.active_bootloaders[self.target].state != ready_state: 
				print('There is an ongoing transaction with targeted bootloader. Do you wish to abort it? [Y/N]')
				answer = input().upper()
				while answer != 'Y' and answer != 'N':
					answer = input('Repeat your answer, please...')

				if answer == 'Y':
					print('Forcing bootloader to abort current transaction ... ')
					self.request_bootloader_exit(self.target, force = True)
				else:
					print('Halting transaction until the ongoing one terminates.')
				while self.listing.active_bootloaders[self.target].state != ready_state:
					time.sleep(1)

			print('Target bootloader is ready.')

		else:
			assert False

		print('\nTransaction starts\n')
		
		print('Sending initial transaction magic ... ', end = '')
		self.send_transaction_magic()
		
		print('Sending the size of new firmware ... ', end = '')
		self.send_handshake(enumerator_by_name("FirmwareSize", self.RegisterEnum), enumerator_by_name("None", self.CommandEnum), self.firmware.length)
		
		print('Sending the number of pages to erase ... ', end = '')
		self.send_handshake(enumerator_by_name('NumPagesToErase', self.RegisterEnum), enumerator_by_name("None", self.CommandEnum), len(self.firmware.influenced_pages))
		
		print('Sending page addresses...')
		for index, page in enumerate(self.firmware.influenced_pages, 1):
			print(f'\r\tErasing page {index:2}/{len(self.firmware.influenced_pages):2} @ 0x{page:08x} ... ', end = '')
			self.send_handshake(enumerator_by_name('PageToErase', self.RegisterEnum), enumerator_by_name("None", self.CommandEnum), page, False)
		print('OK')
		#TODO send one more magic here

		print('Sending entry point address ... ', end = '')
		self.send_handshake(enumerator_by_name('EntryPoint', self.RegisterEnum), enumerator_by_name("None", self.CommandEnum), self.firmware.entry_point)

		print('Sending interrupt vector ... ', end = '')
		#TODO make sure this is really the interrupt vector
		self.send_handshake(enumerator_by_name('InterruptVector', self.RegisterEnum), enumerator_by_name("None", self.CommandEnum), self.firmware.logical_memory_map[0].begin)

		print('Sending second transaction magic ... ', end = '')
		self.send_transaction_magic()

		print('Sending words of firmware...')
		print(f'\tProgress ... {0:05}%', end='')
		checksum = 0
		for block in self.firmware.logical_memory_map:
			assert len(block.data) % 4 == 0 #all messages will carry whole word

			for offset in range(0, len(block.data), 4):
				absolute_address = block.begin + offset
				data = block.data[offset : offset + 4]
				data_as_word = int("".join(data[::-1]), 16)
				#print(f'Programming 0x{absolute_address:08x} = 0x{data_as_word:08x} ... ', end = '')
				self.send_data(absolute_address, data)
				#print('Ok')
				checksum += (data_as_word >> 16) + (data_as_word & 0xffff)
				print(f'\r\tProgress ... {100 * offset / len(block.data):5.2f}%', end='')
			print(f'\r\tProgress ... {100:5.2f}%', end='')

			print(f'\nWritten {len(block.data)} bytes starting from 0x{block.begin:08x}')
		
		print(f'Firmware checksum = 0x{checksum:08x}')

		print('Sending checksum ... ', end = '')
		self.send_handshake(enumerator_by_name('Checksum', self.RegisterEnum), enumerator_by_name("None", self.CommandEnum), checksum)

		print('Sending last transaction magic ... ', end = '')
		self.send_transaction_magic()

		print('Firmware flashed successfully')
		print('Leaving bootloader ... ', end='')
		self.request_bootloader_exit(self.target, force = False)

		self.terminate()

	def terminate(self):
		self.exit = True
		self.ocarinaReadingThread.join()
		

this_terminal = os.ttyname(sys.stdout.fileno())

if args.feature == 'list':
	listing = BootloaderListing(oc, db, this_terminal if args.terminal is None else args.terminal)

	while True:
		ev = oc.read_event(blocking = True)
		assert ev is not None
		listing.process_event(ev)

	listing.terminate()

elif args.feature == 'flash':
	#TODO add error checking if there is no -u or -f
	assert args.unit is not None
	assert args.firmware is not None

	master = FlashMaster(args.unit, args.terminal)
	master.parseFirmware(args.firmware)
	master.flash()

else:
	assert False

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
parser.add_argument('--force', dest='force', action='store_true', help='Terminate ongoing transactions and start a new one')
parser.add_argument('-q', dest='quiet', action='store_true', help='Do not print to standard output.')
parser.add_argument('--verbose', dest='verbose', action='store_true', help='Print absolutely everything.')

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

def find_message(owner, name, database):
	try:
		return next(msg for id, msg in database.parsed_messages.items() if msg.owner == owner and msg.description.name == name)
	except:
		raise Exception(f"Message {owner}::{name} does not exist!")

def find_enum(owner, name, database):
	try:
		return next(e for e in database.parsed_enums if e.name == f'{owner}_{name}')
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
		self.beacon = find_message("Bootloader", "Beacon", self.candb) #all active bootloaders transmit this message
		self.pingResponse = find_message("Bootloader", "PingResponse", self.candb) #whereas units with active firmware transmit this
		self.ping = find_message("Bootloader", "Ping", self.candb)
		self.SoftwareBuild = find_message("Bootloader", "SoftwareBuild", self.candb)
	
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
		self.shall_sleep = False
		self.receiving_acks = False

		if self.terminal is not None:
			self.printThread = threading.Thread(target = BootloaderListing._do_print, args = (self,), daemon=True)
			self.printThread.start()
		self.pingThread = threading.Thread(target = BootloaderListing._do_ping_bootloader_aware_units, args = (self,), daemon=True)
		self.pingThread.start()

	def _do_print(self):
		while not self.exit:
			while self.shall_sleep:
				time.sleep(1)
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
			while self.shall_sleep:
				time.sleep(1)

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
			BLpending = self.pingResponse["BootloaderPending"].value[0]
			target = self.pingResponse["Target"].value[0]

			self.aware_applications[target] = ApplicationData(BLpending, time.time())
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

	def pause(self):
		self.shall_sleep = True

	def resume(self):
		self.shall_sleep = False


def enumerator_by_name(enumerator, enum):
	try:
		return next(val for val, elem in enum.enum.items() if elem.name == enumerator)
	except:
		raise Exception(f"Enumerator {enum.name}::{enumerator} does not exist!")

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

MemoryBlock = namedtuple('MemoryBlock', ['address', 'data'])

class Firmware():

	def __init__(self, path):
		with open(path, 'r') as code:
			self.records = [HexRecord(line.rstrip()) for line in code]

		#Memory map is list of MemoryBlocks (start address + list of bytes)
		self.logical_memory_map, self.entry_point = self.process_hex_records(self.records)
		
		#total size of the firmware in bytes
		self.length = sum(len(block.data) for block in self.logical_memory_map)


	def identify_influenced_physical_blocks(self, physical_memory_map):
		#self.logical_memory_map is certainly in increasing order
		physical_memory_map.sort(key= lambda b: b.address)

		self.influenced_physical_blocks = set()

		logical_index = 0
		address,remaining_bytes = self.logical_memory_map[0]
		remaining_bytes = len(remaining_bytes)

		for physical in physical_memory_map:

			if physical.address + len(physical.data) <= address:
				continue #this physical block ends even berofe the logical starts

			if address < physical.address:
				return False #we could not cover the beginning of this memory block

			#as this point surely holds... physical.address <= logical.address and logical.address < physical.end
			self.influenced_physical_blocks.add(physical)
			if address + remaining_bytes <= physical.address + len(physical.data):
				return True # we have covered logical block

			overlap_size = physical.address + len(physical.data) - address
			remaining_bytes -= overlap_size
			address += overlap_size

			if remaining_bytes == 0:
				logical_index += 1
				if logical_index == len(self.logical_memory_map):
					return True #we have covered all logical blocks

				address, remaining_bytes = self.logical_memory_map[logical_index]
				remaining_bytes = len(remaining_bytes)


		return False

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

			elif absolute_address != current_block.address + len(current_block.data):
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

	def __init__(self, unit, other_terminal, quiet = True):
		self.db = CanDB(args.json_files) #create our own version of CanDB

		self.output_file = sys.stdout if not quiet else open('/dev/null', 'w')

		#get references to enumerations
		self.BootTargetEnum = find_enum("Bootloader", "BootTarget", self.db)
		self.HandshakeResponseEnum = find_enum("Bootloader", "HandshakeResponse", self.db)
		self.RegisterEnum = find_enum("Bootloader", "Register", self.db)
		self.StateEnum = find_enum("Bootloader", "State", self.db)
		self.WriteResultEnum = find_enum("Bootloader", "WriteResult", self.db)
		self.CommandEnum = find_enum('Bootloader', 'Command', self.db)
		self.EntryReasonEnum = find_enum('Bootloader', 'EntryReason', self.db)


		#get references to messages
		self.Ping = find_message("Bootloader", "Ping", self.db)
		self.PingResponse = find_message("Bootloader", "PingResponse", self.db)
		self.ExitReq = find_message("Bootloader", "ExitReq", self.db)
		self.ExitAck = find_message("Bootloader", "ExitAck", self.db)
		self.Beacon = find_message("Bootloader", "Beacon", self.db)
		self.Data = find_message("Bootloader", "Data", self.db)
		self.DataAck = find_message("Bootloader", "DataAck", self.db)
		self.Handshake = find_message("Bootloader", "Handshake", self.db)
		self.HandshakeAck = find_message("Bootloader", "HandshakeAck", self.db)
		self.CommunicationYield = find_message('Bootloader', 'CommunicationYield', self.db)
		self.SoftwareBuild = find_message('Bootloader', 'SoftwareBuild', self.db)

		self.targetName = unit
		self.target = enumerator_by_name(self.targetName, self.BootTargetEnum)
		self.exit = False
		self.isBusMaster = True


		self.listing = BootloaderListing(oc, db, other_terminal if not quiet else '/dev/null')
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
					print('Bootloader aborted transaction!', file=self.output_file)
					sys.exit(1) #todo this deserves a bit nicer handling

	def receive_generic_response(self, id, sent : tuple, must_match : list, wanted : list):
		message = self.db.getMsgById(id)

		for field in must_match:
			if sent._asdict()[field] != message[field].value[0]:
				print(f'Received unrelated acknowledge!', file=sys.stderr)
				return False

		return [message[field].value[0] for field in wanted]

	def get_next_message(self, searched_id = None):
		while True:
			#receive message and check whether we are interested
			ev = self.message_queue.get()
			assert ev is not None
			if not isinstance(ev, ocarina.CanMsgEvent):
				continue

			#extract information from message's bits
			self.db.parseData(ev.id.value, ev.data, ev.timestamp)

			if searched_id is None:
				return ev # we are waiting for any message

			if ev.id.value == searched_id:
				return ev # we are waiting for a single specific one

	def wait_for_response(self, expected_id, sent):
		while True:
			ev = self.get_next_message()

			if ev.id.value in (self.SoftwareBuild.identifier, self.Beacon.identifier):
				continue #these messages are not part of protocol - ignore them

			if ev.id.value != expected_id:
				continue

			if ev.id.value == self.HandshakeAck.identifier:
				return self.receive_generic_response(self.HandshakeAck.identifier, sent, ['Register', 'Value'], ['Response'])
			elif ev.id.value == self.Handshake.identifier:
				assert False #shall not be received here
			elif ev.id.value == self.CommunicationYield.identifier:
				assert False #shall not be received here
			elif ev.id.value == self.Data.identifier:
				assert False #shall not be received yet
			elif ev.id.value == self.DataAck.identifier:
				return self.receive_generic_response(self.DataAck.identifier, sent, ['Address'], ['Result'])
			elif ev.id.value == self.ExitAck.identifier:
				return self.receive_generic_response(self.ExitAck.identifier, sent, ['Target'], ['Confirmed'])
			elif ev.id.value == self.PingResponse.identifier:
				return self.receive_generic_response(self.PingResponse.identifier, sent, ['Target'], ['BootloaderPending'])


	def send_generic_message_and_await_response(self, id : int, data : list, responseID : int, await_response = True):
		message = db.getMsgById(id)
		buffer = [0] * message.length

		sent = message.namedtuple(*data)
		message.assemble(data, buffer)
		oc.send_message_std(id, buffer)


		if await_response:
			return self.wait_for_response(responseID, sent)

	def send_handshake(self, reg, command, value):
		r = enumerator_by_name(reg, self.RegisterEnum)
		c = enumerator_by_name(command, self.CommandEnum)
		return self.send_generic_message_and_await_response(self.Handshake.identifier, [r, c, value], self.HandshakeAck.identifier)[0]

	def send_data(self, address, word):
		return self.send_generic_message_and_await_response(self.Data.identifier, [address, False, word], self.DataAck.identifier)[0]

	def send_handshake_ack(self, reg, response, value):
		return self.send_generic_message_and_await_response(self.HandshakeAck.identifier, [reg, response, value], None, False)

	def yield_communication(self):
		self.isBusMaster = False
		return self.send_generic_message_and_await_response(self.CommunicationYield.identifier, [self.target], None, False)

	def send_transaction_magic(self):
		return self.send_handshake("TransactionMagic", "None", FlashMaster.transactionMagic)

	def send_command(self, command):
		return self.send_handshake("Command", command, 0)


	def request_bootloader_exit(self, target, force):
		return self.send_generic_message_and_await_response(self.ExitReq.identifier, [target, force], self.ExitAck.identifier)[0]

	def request_bootloader_entry(self, target):
		return self.send_generic_message_and_await_response(self.Ping.identifier, [target, True], self.PingResponse.identifier)[0]

	def print_header(self):
		print('Desktop interface to CAN Bootloader', file=self.output_file)
		print('Written by Vojtech Michal, (c) eForce FEE Prague Formula 2020\n', file=self.output_file)

		if not args.verbose:
			return

		targets = [elem.name for val, elem in self.BootTargetEnum.enum.items()]
		if len(targets) == 1:
			print(f"The only targetable unit is {targets[0]}", file=self.output_file)
		else:
			print(f"Targetable units are {{{', '.join(targets)}}}", file=self.output_file)

	def parseFirmware(self, firmwarePath):
		if args.verbose:
			print(f'Parsing hex file {firmwarePath}', file=self.output_file)
		self.firmware = Firmware(firmwarePath)
		if args.verbose:
			print(f'Input consists of {len(self.firmware.records)} hex records.', file=self.output_file)

		print(f'Firmware logical memory map ({self.firmware.length} bytes in total):', file=self.output_file)
		for block in self.firmware.logical_memory_map:
			print(f'\t[0x{block.address:08x} - 0x{block.address + len(block.data) - 1:08x}] ... {len(block.data):8} B ({len(block.data)/1024:.2f} KiB)', file=self.output_file)
		print(f'Entry point: 0x{(self.firmware.entry_point & ~1):08x} ({"Thumb" if self.firmware.entry_point & 1 else "ARM"} mode)', file=self.output_file)
		print('', file=self.output_file)

	def report_handshake_response(self, response):
		if args.verbose:
			print(self.HandshakeResponseEnum.enum[response].name, file=self.output_file)

		if response != enumerator_by_name('OK', self.HandshakeResponseEnum):
			print(f'Handshake failed with code {self.HandshakeResponseEnum.enum[response].name}. Exiting', file=sys.stderr)
			#TODO maybe request the bootloader to exit as well
			sys.exit(3) #kill the program if handshake failed

	def checkCommand(self, reg, command):
		if reg != enumerator_by_name('Command', self.RegisterEnum) and command != enumerator_by_name('None', self.CommandEnum):
			return enumerator_by_name('CommandNotNone', self.HandshakeResponseEnum)
		return enumerator_by_name('OK', self.HandshakeResponseEnum)

	def checkMagic(self, reg, command, value):
		if self.checkCommand(reg, command) != enumerator_by_name('OK', self.HandshakeResponseEnum):
			return self.checkCommand(reg, command)

		if reg != enumerator_by_name('TransactionMagic', self.RegisterEnum):
			return enumerator_by_name('HandshakeSequenceError', self.HandshakeResponseEnum)

		if value != FlashMaster.transactionMagic:
			return enumerator_by_name('InvalidTransactionMagic', self.HandshakeResponseEnum)

		return enumerator_by_name('OK', self.HandshakeResponseEnum)

	def receive_handshake(self, register_name):
		register = enumerator_by_name(register_name, self.RegisterEnum)
		while True: 
			self.get_next_message(self.Handshake.identifier)
			reg, command, value = map(lambda f: f.value[0], self.Handshake.fields)

			if reg != register:
				self.send_handshake_ack(reg, SeqError, value)
				continue

			self.send_handshake_ack(reg, enumerator_by_name('OK', self.HandshakeResponseEnum), value)
			return value

	def receive_transaction_magic(self):
		while True:
			self.get_next_message(self.Handshake.identifier)
			reg, command, value = map(lambda f: f.value[0], self.Handshake.fields)

			res = self.checkMagic(reg, command, value)
			self.send_handshake_ack(reg, res, value)
			if res == enumerator_by_name('OK', self.HandshakeResponseEnum):
				return #transaction magic is ok, we can go further


	def flash(self, force = False):
		if args.verbose:
			print(f"Initiating flash process for {self.targetName} (target ID {self.target})\n", file=self.output_file)
		self.ocarinaReadingThread.start()

		print('Searching bootloader aware units present on the bus...', file=self.output_file)

		is_bootloader_active = lambda target: target in self.listing.active_bootloaders
		is_application_active = lambda target: target in self.listing.aware_applications and self.listing.aware_applications[target].last_response is not None

		#wait for the target unit to show up
		while not is_bootloader_active(self.target) and not is_application_active(self.target):
			time.sleep(0.1)
		
		if args.verbose:
			print("Target's presence on the CAN bus confirmed.", file=self.output_file)

		#the target has application running -> request it to reset into bootloader
		if is_application_active(self.target):
			print(f'Waiting for {self.targetName} to enter bootloader.', file=self.output_file)

			for i in range(5):
				if self.request_bootloader_entry(self.target):
					break
			else:
				print('Target unit refused to enter the bootloader. Exiting', file=self.output_file)
				return
			#await the bootloader's activation
			while not is_bootloader_active(self.target): 
				time.sleep(0.5)

		elif is_bootloader_active(self.target):
			print('Target is already in bootloader.', file=self.output_file)
			ready_state = enumerator_by_name('Ready', self.StateEnum)
			#if the bootloader is not ready, there is another transaction
			if self.listing.active_bootloaders[self.target].state != ready_state: 
				if args.verbose:
					print('There is an ongoing transaction with targeted bootloader. ', end='', file=self.output_file)
				if not force: #we are not allowed to interfere into that transaction
					print('Specify --force during program invocation to abort ongoing transaction.', file=self.output_file)
					return 

				if args.verbose:
					print('Forcing bootloader to abort current transaction ... ', file=self.output_file)
				if not self.request_bootloader_exit(self.target, force = True):
					print('Bootloader refused to abort ongoing transaction.', file=self.output_file)
					return
				#wait for the bootloader to become ready
				while self.listing.active_bootloaders[self.target].state != ready_state:
					time.sleep(0.5)

			if args.verbose:
				print('Target bootloader is ready.', file=self.output_file)

		else:
			assert False

		if args.verbose:
			print('\nTransaction starts\n', file=self.output_file)
			print('Sending initial transaction magic ... ', end='', file=self.output_file)

		self.report_handshake_response(self.send_transaction_magic())

		if args.verbose:
			print('Initiating flashing transaction ... ', end = '', file=self.output_file)
		self.report_handshake_response(self.send_command('StartTransactionFlashing'))
		
		if args.verbose:
			print('Communication yields...', file=self.output_file)
		self.yield_communication()

		#receive initial transaction magic
		self.receive_transaction_magic()

		#receive the number of physical memory blocks
		physical_memory_map = [0] * int(self.receive_handshake('NumPhysicalMemoryBlocks'))

		for i in range(len(physical_memory_map)): #receiving blocks
			start = int(self.receive_handshake('PhysicalBlockStart'))
			length = int(self.receive_handshake('PhysicalBlockLength'))

			physical_memory_map[i] = MemoryBlock(start, 'x' * length)

		#receive terminal transaction magic
		self.receive_transaction_magic()

		if args.verbose:
			print('Awaiting bootloader yield ... ', end= '', file=self.output_file)
		self.get_next_message(self.CommunicationYield.identifier)
		if args.verbose:
			print('OK\n\n', file=self.output_file)

		if not self.firmware.identify_influenced_physical_blocks(physical_memory_map):
			print('Available physcial memory cannot cover all address ranges required by firmware! Exiting.', file=self.output_file)
			return

		##Transmission of logical memory map

		if args.verbose:
			print('Sending initial magic ... ', end='', file=self.output_file)
		self.report_handshake_response(self.send_transaction_magic()) #initial magic

		print(f'Sending number of logical memory blocks ({len(self.firmware.logical_memory_map)}) ... ', end='' if args.verbose else '\n', file=self.output_file)
		self.report_handshake_response(self.send_handshake('NumLogicalMemoryBlocks', 'None', len(self.firmware.logical_memory_map)))

		if args.verbose:
			print(f'Sending individual logical memory blocks ... ', file=self.output_file)

		for block in self.firmware.logical_memory_map: #send blocks one by one
			if args.verbose:
				print(f'Start ', end = '', file=self.output_file)
			self.report_handshake_response(self.send_handshake('LogicalBlockStart', 'None', block.address))

			if args.verbose:
				print(f'length ', end='', file=self.output_file)
			self.report_handshake_response(self.send_handshake('LogicalBlockLength', 'None', len(block.data)))

		if args.verbose:
			print('Sending terminal magic ... ', end='', file=self.output_file)
		self.report_handshake_response(self.send_transaction_magic()) #terminal magic

		if args.verbose:
			print('Sent firmware memory map.\n\n', file=self.output_file)

		##Physical block erassure

		if args.verbose:
			print(f'Sending {len(self.firmware.influenced_physical_blocks)} page addresses ... ', file=self.output_file)
		self.report_handshake_response(self.send_transaction_magic()) #initial magic
		self.report_handshake_response(self.send_handshake('NumPhysicalBlocksToErase', 'None', len(self.firmware.influenced_physical_blocks)))
		for index, page in enumerate(self.firmware.influenced_physical_blocks, 1):
			print(f'\r\tErasing page {index:2}/{len(self.firmware.influenced_physical_blocks):2} @ 0x{page.address:08x} ... ', end = '', file=self.output_file)
			result = self.send_handshake('PhysicalBlockToErase', "None", page.address)
			if result != enumerator_by_name('OK', self.HandshakeResponseEnum):
				print(f"Page {index} erassure returned result {self.HandshakeResponseEnum.enum[result].name}", file = sys.stderr)
		print('OK', file=self.output_file)
		self.report_handshake_response(self.send_transaction_magic()) #terminal magic

		##Firmware download

		if args.verbose:
			print('Sending initial magic ... ', end = '', file=self.output_file)
		self.report_handshake_response(self.send_transaction_magic())

		print(f'Sending firmware size ({self.firmware.length})... ', end = '' if args.verbose else '\n', file=self.output_file)
		self.report_handshake_response(self.send_handshake('FirmwareSize', 'None', self.firmware.length))

		print('Sending words of firmware...', file=self.output_file)
		print(f'\tProgress ... {0:05}%', end='', file=self.output_file)
		checksum = 0
		start = time.time()
		sent_bytes = 0
		last_print = time.time() - 1

		if args.quiet:
			self.listing.pause()
		for block in self.firmware.logical_memory_map:
			assert len(block.data) % 4 == 0 #all messages will carry whole word

			for offset in range(0, len(block.data), 4):
				absolute_address = block.address + offset
				data = block.data[offset : offset + 4]
				data_as_word = sum(byte << index*8 for index, byte in enumerate(data))
				result = self.send_data(absolute_address, data_as_word)
				if result != enumerator_by_name('Ok', self.WriteResultEnum):
					print(f'Write returned {self.WriteResultEnum.enum[result].name}', file = sys.stderr)

				checksum += (data_as_word >> 16) + (data_as_word & 0xffff)
				sent_bytes += 4
				if time.time() - last_print > 1:
					print(f'\r\tProgress ... {100 * offset / len(block.data):5.2f}% ({sent_bytes/1024/(time.time()-start):2.2f} KiBps)', end='', file=self.output_file)
					last_print = time.time()
			print(f'\r\tProgress ... {100:5.2f}% ({sent_bytes/1024/(time.time()-start):2.2f} KiBps)', end='', file=self.output_file)

			print(f'\nWritten {len(block.data)} bytes starting from 0x{block.address:08x}', file=self.output_file)
		print(f'Took {(time.time() - start)*1000:.2f} ms.')
		print(f'Firmware checksum = 0x{checksum:08x}', file=self.output_file)

		if args.quiet:
			self.listing.resume()

		if args.verbose:
			print('Sending checksum ... ', end = '', file=self.output_file)
		self.report_handshake_response(self.send_handshake('Checksum', "None", checksum))

		if args.verbose:
			print('Sending terminal transaction magic ... ', end = '', file=self.output_file)
		self.report_handshake_response(self.send_transaction_magic())

		##Firmware metadata

		if args.verbose:
			print('Sending initial transaction magic ... ', end = '', file=self.output_file)
		self.report_handshake_response(self.send_transaction_magic())

		if args.verbose:
			print('Sending interrupt vector ... ', end = '', file=self.output_file)
		#TODO make sure this is really the interrupt vector
		self.report_handshake_response(self.send_handshake('InterruptVector', "None", self.firmware.logical_memory_map[0].address))

		if args.verbose:
			print('Sending entry point address ... ', end = '', file=self.output_file)
		self.report_handshake_response(self.send_handshake('EntryPoint', "None", self.firmware.entry_point))

		if args.verbose:
			print('Sending terminal transaction magic ... ', end = '', file=self.output_file)
		self.report_handshake_response(self.send_transaction_magic())

		if args.verbose:
			print('Firmware flashed successfully', file=self.output_file)
		print('Leaving bootloader. ', end='' if args.verbose else '\n', file=self.output_file)

		result = self.request_bootloader_exit(self.target, force = False)
		if args.verbose:
			print('Confirmed' if result else 'Declined', file=self.output_file)

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

	master = FlashMaster(args.unit, args.terminal, args.quiet)
	master.parseFirmware(args.firmware)
	master.flash(args.force)

else:
	assert False

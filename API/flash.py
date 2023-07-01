#!/usr/bin/env python3

memory_map_print_elems_per_line = 4

import os
import struct
import argparse
import sys
import threading
import itertools
import queue
from collections import namedtuple

parser = argparse.ArgumentParser(description=
"""=========== CAN bootloader ============
Desktop interface to CAN bootloaders embedded into the electric formula.
Written by Vojtech Michal, (c) eForce FEE Prague Formula 2020, 2021

Possible ways you may want to use this API:
List all bootloader aware units present on the bus:
	python3.8 flash.py -j path_to_json -f list /dev/ttyS4

Flash an ECU with new firmware in the simplest way possible
	python3.8 flash.py -j path_to_json -f flash -u AMS -x path_to_hex /dev/ttyS4

Flash an ECU with new firmware with fancy UI, terminating all previous transactions
	python3.8 flash.py -j path_to_json -f flash -u AMS -x path_to_hex -t /dev/tty7 --force --verbose /dev/ttyS4

Update the bootloader itself (optionally using fancy UI and terminating previous transactions):
	python3.8 flash.py -j path_to_json -f update_bootloader -u AMS -x path_to_hex --force --verbose /dev/ttyS3

Make the BL accept already flashed firmware with isr vector located at 0x0800'3000
	python3.8 flash.py -j path_to_json -f set_vector_table --address 0x08003000 -u AMS /dev/ttyS3     

Request the active bootloader to exit and start the application
	python3.8 flash.py -j path_to_json -f exit -u AMS /dev/ttyS3	

Request the application to enter the bootloader.   
	python3.8 flash.py -j path_to_json -f enter -u AMS /dev/ttyS3""",\
	formatter_class=argparse.RawTextHelpFormatter)

parser.add_argument('-j', metavar="file", dest='json_files', type=str, action='append', help='add candb json file to parse')
parser.add_argument('-f', dest='feature', type=str, choices=["list", "flash", "set_vector_table", "enter", "exit", "update_bootloader"], default="list", help='choose feature - list bootloader aware units or flash new firmware')
parser.add_argument('--address', dest='address', type=str, help='absolute memory address to use (address of vector table to store when using -f set_vector_table)')
parser.add_argument('-u', dest='unit', type=str, help='Unit to flash.')
parser.add_argument('-x', dest='firmware', type=str, help='Path to hex file for flashing')
parser.add_argument('-t', dest='terminal', type=str, help='Path to second terminal for pretty bootloader listing')
parser.add_argument('--force', dest='force', action='store_true', help='Terminate all ongoing transactions and start a new one')
parser.add_argument('-q', dest='quiet', action='store_true', help='Do not print to standard output.')
parser.add_argument('--verbose', dest='verbose', action='store_true', help='Print absolutely everything.')

parser.add_argument('ocarina',  metavar='ocarina', type=str, help='path to ocarina COM port device (/dev/ocarina, /dev/ttyUSB0,...)')

args = parser.parse_args()

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

TargetBootloaderData = namedtuple('TargetBootloaderData', ['state', 'flash_size', 'last_response', 'entry_reason'])
TargetSoftwareBuild = namedtuple('TargetSoftwareBuild', ['CommitSHA', 'DirtyRepo'])
ApplicationData = namedtuple('ApplicationData', ['BLpending', 'last_response'])

class BootloaderListing:
	def __init__(self, oc, canDB, terminal):
		self.terminal_name = terminal
		self.terminal = open(terminal, 'w') if terminal is not None else None

		self.oc = oc
		self.candb = canDB
		self.beacon = self.candb.getMsgByName("Bootloader", "Beacon") #all active bootloaders transmit this message
		self.pingResponse = self.candb.getMsgByName("Bootloader", "PingResponse") #whereas units with active firmware transmit this
		self.ping = self.candb.getMsgByName("Bootloader", "Ping")
		self.SoftwareBuild = self.candb.getMsgByName("Bootloader", "SoftwareBuild")
	
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
		self.active_bootloaders  = { } # dictionary BootTarget : TargetBootloaderData
		self.bootloader_builds = { } # dictionary BootTarget : TargetSoftwareBuild
		self.application_builds = { } # dictionary name (string) : TargetSoftwareBuild
		self.aware_applications  = { unit : ApplicationData(None, None) for unit in self.targetable_units }

		self.exit = False
		self.shall_sleep = False
		self.receiving_acks = False
		self.busBitrate = None

		if self.terminal is not None:
			self.printThread = threading.Thread(target = BootloaderListing._do_print, args = (self,), daemon=True)
			self.printThread.start()

		self.pingCyclePeriodSlow = 1.0
		self.pingCyclePeriodFast = 0.05

		self.pingCyclePeriod = self.pingCyclePeriodFast # Start fast by default to catch the bootloader during BL startup CAN bus check
		self.pingThread = threading.Thread(target = BootloaderListing._do_ping_bootloader_aware_units, args = (self,), daemon=True)
		self.pingThread.start()

	def _generate_sw_build_string_for(self, unit, bl_build):
		relevant_dict = self.bootloader_builds

		if not bl_build: #application builds are indexed by string name
			relevant_dict = self.application_builds
			unit = self._get_unit_name(unit)

		if unit in relevant_dict:
			commit = relevant_dict[unit]

			if commit.CommitSHA is not None:
				return f'0x{commit.CommitSHA:08x}{"(dirty)" if commit.DirtyRepo else ""}'
		return ''

	def _get_unit_name(self, unit):
		return self.beacon["Target"].linked_enum.enum[unit].name

	def _do_print(self):
		while not self.exit:
			while self.shall_sleep:
				time.sleep(1)
			assert self.terminal is not None
			time.sleep(1)
			clearscreen(self.terminal_name)

			if not self.receiving_acks:
				print('WARNING: Ocarina is not receving acknowledge frames!\n', file = self.terminal)

			print(f'State of bootloader aware units (bitrate {self.busBitrate if self.busBitrate is not None else "N/A"} kbitps):\n', file = self.terminal)
			if len(self.aware_applications) == 0 and len(self.active_bootloaders) == 0:
				print('None', file = self.terminal)
				continue

			targets = []
			states = []
			flash_sizes = []
			entry_reasons = []
			last_response = []

			#copy data to separate lists
			for unit, data in self.active_bootloaders.items():
				targets.append(unit)

				flash_sizes.append(str(data.flash_size))
				entry_reasons.append(self.beacon["EntryReason"].linked_enum.enum[data.entry_reason].name)

				last_response.append("yep" if time.time() - data.last_response < 2.0 else "connection lost")
				states.append(self.beacon["State"].linked_enum.enum[data.state].name)

			for unit, data in self.aware_applications.items():
				targets.append(unit)

				flash_sizes.append('')
				entry_reasons.append('')

				if data.last_response is not None:
					last_response.append("yep" if time.time() - data.last_response < 2.0 else "connection lost")
					state = "FirmwareRunning" if not data.BLpending else "BLpending"
				else:
					last_response.append("Never seen")
					state = 'Unknown'

				states.append(state)

			bl_builds = [self._generate_sw_build_string_for(target, True) for target in targets]
			app_builds = [self._generate_sw_build_string_for(target, False) for target in targets]
			targets = [self._get_unit_name(unit) for unit in targets]

			#find longest strings in each list
			space_width = 6
			length_target    = space_width + max(max(map(lambda s:len(s), targets))         , len("Target"))
			length_state     = space_width + max(max(map(lambda s:len(s), states))          , len("State"))
			length_reason    = space_width + max(max(map(lambda s:len(s), entry_reasons))   , len("Activation reason"))
			length_flash     = space_width + len("Flash [KiB]")
			length_bl_build  = space_width + max(max(map(lambda s:len(s), bl_builds))         , len("BL build"))
			length_app_build  = space_width + max(max(map(lambda s:len(s), app_builds))         , len("APP build"))
			length_connected = len("connection lost")
	
			print(f'{"Target":{length_target}}{"State":{length_state}}{"BL build":{length_bl_build}}{"APP build":{length_app_build}}{"Flash [KiB]":{length_flash}}{"Activation reason":{length_reason}}{"Responding?":{length_connected}}', file = self.terminal)
			print('-' * (length_target + length_state + length_reason + length_flash + length_connected + length_bl_build + length_app_build), file = self.terminal)
			for target, state, reason, flash_size, bl_build, app_build, response in zip(targets, states, entry_reasons, flash_sizes, bl_builds, app_builds, last_response):
				print(f'{target:{length_target}}{state:{length_state}}{bl_build:{length_bl_build}}{app_build:{length_app_build}}{flash_size:{length_flash}}{reason:{length_reason}}{response:{length_connected}}', file = self.terminal)
		
	def _do_ping_bootloader_aware_units(self):
		#cycle through all possible boot targets and ping them
		while not self.exit:
			oc.query_config() # poll the ocarina's bitrate 
			while self.shall_sleep:
				time.sleep(1)

			targets = list(filter(lambda t: t not in self.active_bootloaders, self.targetable_units)) #target only those units that are not in bootloader

			for unit in targets:
				buffer = [0] * 1
				self.ping.assemble([unit, False], buffer)
	
				if self.receiving_acks: #there is some other node on the bus
					self.oc.send_message_std(self.ping.identifier, buffer)
				time.sleep(self.pingCyclePeriod / len(targets))

	def process_event(self, ev):
		if isinstance(ev, ocarina.CANErrorEvent): #there is some error on CAN
			if ev.eType.value == ocarina.CANERROR.ACKNOWLEDGMENT:
				self.receiving_acks = False
			return

		elif isinstance(ev, ocarina.HeartbeatEvent) or isinstance(ev, ocarina.ErrorFlagsEvent):
			return#ignore HeartbeatEvent

		elif isinstance(ev, ocarina.ConfigEvent):
			if self.busBitrate is None:
				self.busBitrate = ev.bitrate.kbits
			return
	
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


			self.active_bootloaders[target] = TargetBootloaderData(state, flash_size, time.time(), entry_reason)
			if target in self.aware_applications:
				del self.aware_applications[target] #target has entered bootloader -> remove it from list of aware applications
	
		elif ev.id.value == self.pingResponse.identifier: #some bootloader aware unit responded
			#extract information from message's bits
			self.candb.parseData(ev.id.value, ev.data, ev.timestamp)
			BLpending = self.pingResponse["BootloaderPending"].value[0]
			target = self.pingResponse["Target"].value[0]
			self.aware_applications[target] = ApplicationData(BLpending, time.time())

			if self.pingResponse['BootloaderMetadataValid'].value[0]:
				bl_build = int(self.pingResponse['BL_SoftwareBuild'].value[0])
				bl_dirty = bool(self.pingResponse['BL_DirtyRepo'].value[0])
				self.bootloader_builds[target] = TargetSoftwareBuild(bl_build, bl_dirty)

			if target in self.active_bootloaders: #remove the target from list of active bootloaders
				del self.active_bootloaders[target]


		elif ev.id.value == self.SoftwareBuild.identifier:
			#extract information from message's bits
			self.candb.parseData(ev.id.value, ev.data, ev.timestamp)

			target = self.SoftwareBuild["Target"].value[0]
			sha = int(self.SoftwareBuild["CommitSHA"].value[0])
			dirty = bool(self.SoftwareBuild["DirtyRepo"].value[0])

			self.bootloader_builds[target] = TargetSoftwareBuild(sha, dirty)

		elif self.candb.isMsgKnown(ev.id.value): # handle all other known messages. We are interested in messages called 'SoftwareBuild'
			msg = self.candb.getMsgById(ev.id.value)
			if msg.name != "SoftwareBuild":
				return
			self.candb.parseData(ev.id.value, ev.data, ev.timestamp)

			sha = int(msg['CommitSHA'].value[0])
			dirty = bool(msg['DirtyRepo'].value[0])
			#TODO this may require some minor adjustment of msg.owner to use name recognized by printing logic
			self.application_builds[msg.owner] = TargetSoftwareBuild(sha, dirty)



	def terminate(self):
		self.exit = True

		self.pingThread.join()
		if self.terminal is not None:
			self.printThread.join()

	def pause(self):
		self.shall_sleep = True

	def resume(self):
		self.shall_sleep = False


def enumerator_by_name(enumerator, enum):
	try:
		return next(val for val, elem in enum.enum.items() if elem.name == enumerator)
	except:
		raise Exception(f"Enumerator {enum.name}::{enumerator} does not exist!")

# For documentation of the Intel HEX format, see https://en.wikipedia.org/wiki/Intel_HEX
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
		elif self.isStartLinearAddressRecord() or self.isStartSegmentAddressRecord():
			assert self.length == 4

	def isDataRecord(self):
		return self.type == 0

	def isEofRecord(self):
		return self.type == 1

	def isExtendedSegmentAddressRecord(self):
		return self.type == 2

	# Supported only experimentally, as Daník's toolchain keeps generating this type of record
	def isStartSegmentAddressRecord(self):
		return self.type == 3

	def isExtendedLinearAddressRecord(self):
		return self.type == 4

	def isStartLinearAddressRecord(self):
		return self.type == 5

MemoryBlock = namedtuple('MemoryBlock', ['address', 'data'])

class Firmware:

	def __init__(self, path):
		with open(path, 'r') as code:
			self.records = [HexRecord(line.rstrip()) for line in code]

		#Memory map is list of MemoryBlocks (start address + list of bytes)
		self.logical_memory_map, self.entry_point = self.process_hex_records(self.records)
		self.base_address = self.logical_memory_map[0].address
		self.end = self.logical_memory_map[-1].address + len(self.logical_memory_map[-1].data)


		self.flattened_map = [None] * (self.end - self.base_address)
		for block in self.logical_memory_map:
			offset = block.address - self.base_address
			self.flattened_map[offset: offset+ len(block.data)] = block.data
		
		#total size of the firmware in bytes
		self.length = sum(len(block.data) for block in self.logical_memory_map)

	def calculate_checksum(self):
		to_half = lambda data: sum(byte << (8*index) for index, byte in enumerate(data))
		sum_block = lambda block: sum(map(to_half, zip(block.data[0::2], block.data[1::2])))

		assert all(len(block)%2 == 0 for block in self.logical_memory_map)
		return sum(map(sum_block, self.logical_memory_map))

	def identify_influenced_physical_blocks(self, physical_memory_map):
		#self.logical_memory_map is certainly in increasing order
		physical_memory_map.sort(key= lambda b: b.address)
		influenced_physical_blocks = []

		logical_index = 0
		address,remaining_bytes = self.logical_memory_map[0]
		remaining_bytes = len(remaining_bytes)

		physical_offset = 0

		while True:
			if physical_offset == len(physical_memory_map):
				return None #We have run out of physical blocks without covering the whole firmware!
			physical = physical_memory_map[physical_offset]

			if physical.address + len(physical.data) <= address:
				physical_offset += 1
				continue #this physical block ends even before the logical starts

			if address < physical.address:
				return None #we could not cover the beginning of this memory block

			#as this point surely holds... physical.address <= logical.address and logical.address < physical.end
			influenced_physical_blocks.append(physical)

			overlap_size = physical.address + len(physical.data) - address


			remaining_bytes -= overlap_size
			address += overlap_size

			if remaining_bytes <= 0:
				logical_index += 1
				if logical_index == len(self.logical_memory_map):
					return influenced_physical_blocks #we have covered all logical blocks

				address, remaining_bytes = self.logical_memory_map[logical_index]
				remaining_bytes = len(remaining_bytes)
			else:
				physical_offset += 1

				if physical_offset == len(physical_memory_map):
					return influenced_physical_blocks #we have covered all logical blocks

		return None

	#takes a list of HexRecords and returns a list of MemoryBlocks
	#returned list models the firmware's memory map
	@staticmethod
	def process_hex_records(records : list):
		if not records.pop().isEofRecord(): # EOF record must be the last thing in the file
			raise Exception('Input file was not terminated by EOF hex record!')

		logical_memory_map = []

		entry_point = None
		current_block = None
		base_address = 0 # set by extended linear address record


		for index, record in enumerate(records, start=1):

			if record.isStartLinearAddressRecord(): #we have address of the entry point
				entry_point = int(record.data, 16) #store it to send it to the bootloader later
				continue
			elif record.isStartSegmentAddressRecord():
				entry_point = int(record.data, 16)
				print(f"Received entry point 0x{entry_point:08x} from StartSegmentAddress record (type 3). This is an experimental feature, contact Vojtěch Michal!")
				continue
			elif record.isExtendedLinearAddressRecord():
				base_address = int(record.data, 16) << 16
				continue
			elif record.isExtendedSegmentAddressRecord():
				base_address = int(record.data, 16) << 4
				continue
			elif record.isEofRecord():
				raise Exception(f'There shall not be multiple eof records in a valid input file (spotted on line {index}).')

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

		return logical_memory_map, entry_point

class FlashMaster:

	transactionMagicString = "Heli"
	transactionMagic = sum(ord(char) << 8*index for index, char in enumerate(transactionMagicString))

	def __init__(self, unit, other_terminal, quiet = True):
		self.db = CanDB(args.json_files) #create our own version of CanDB

		self.output_file = sys.stdout if not quiet else open('/dev/null', 'w')

		#get references to enumerations
		self.BootTargetEnum = self.db.getEnumByName("Bootloader", "BootTarget")
		self.HandshakeResponseEnum = self.db.getEnumByName("Bootloader", "HandshakeResponse")
		self.RegisterEnum = self.db.getEnumByName("Bootloader", "Register")
		self.StateEnum = self.db.getEnumByName("Bootloader", "State")
		self.WriteResultEnum = self.db.getEnumByName("Bootloader", "WriteResult")
		self.CommandEnum = self.db.getEnumByName('Bootloader', 'Command')
		self.EntryReasonEnum = self.db.getEnumByName('Bootloader', 'EntryReason')


		#get references to messages
		self.Ping = self.db.getMsgByName("Bootloader", "Ping")
		self.PingResponse = self.db.getMsgByName("Bootloader", "PingResponse")
		self.ExitReq = self.db.getMsgByName("Bootloader", "ExitReq")
		self.ExitAck = self.db.getMsgByName("Bootloader", "ExitAck")
		self.Beacon = self.db.getMsgByName("Bootloader", "Beacon")
		self.Data = self.db.getMsgByName("Bootloader", "Data")
		self.DataAck = self.db.getMsgByName("Bootloader", "DataAck")
		self.Handshake = self.db.getMsgByName("Bootloader", "Handshake")
		self.HandshakeAck = self.db.getMsgByName("Bootloader", "HandshakeAck")
		self.CommunicationYield = self.db.getMsgByName("Bootloader", "CommunicationYield")
		self.SoftwareBuild = self.db.getMsgByName("Bootloader", "SoftwareBuild")

		self.targetName = unit
		self.target = enumerator_by_name(self.targetName, self.BootTargetEnum)
		self.exit = False
		self.isBusMaster = True

		self.busBitrate = None
		self.stallRequested = False
		self.stallStart = None
		self.totalTimeStalled = 0.0
		self.currentDataOffset = None
		self.dataTransmissionFinished = False
		self.dataTransmissionInProgress = False

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

			if isinstance(ev, ocarina.ConfigEvent):
				if self.busBitrate is None:
					self.busBitrate = ev.bitrate.kbits
				elif self.busBitrate != ev.bitrate.kbits:
					print(f'Bus bitrate has changed from {self.busBitrate} to {ev.bitrate.kbits} KiBps!', file = sys.stderr)

			elif isinstance(ev, ocarina.CanMsgEvent) and ev.id.value == self.DataAck.identifier:
				self.db.parseData(ev.id.value, ev.data, ev.timestamp)
				#data acknowledge informs us that the bootloader is satisfied with all received data
				assert self.DataAck['Result']
				if self.DataAck['Result'].value[0] == enumerator_by_name('Ok', self.WriteResultEnum):
					self.dataTransmissionFinished = True

			elif isinstance(ev, ocarina.CanMsgEvent) and ev.id.value == self.Handshake.identifier: #test for important handshake messages...
				self.db.parseData(ev.id.value, ev.data, ev.timestamp)
				#sanity check that the command field is cleared iff the register field is other than command
				if self.Handshake['Register'].value[0] != enumerator_by_name('Command', self.RegisterEnum):
					assert self.Handshake['Command'].value[0] == enumerator_by_name('None', self.CommandEnum)

				assert self.Handshake['Command']
				if self.Handshake['Command'].value[0] == enumerator_by_name('StallSubtransaction', self.CommandEnum):
					self.stallRequested = True
					self.stallStart = time.time()

				elif self.Handshake['Command'].value[0] == enumerator_by_name('ResumeSubtransaction', self.CommandEnum):
					self.stallRequested = False
					self.totalTimeStalled += time.time() - self.stallStart
					self.stallStart = None
				
				elif self.Handshake['Command'].value[0] == enumerator_by_name('RestartFromAddress', self.CommandEnum):
					if self.dataTransmissionInProgress:
						self.stallRequested = True
						time.sleep(0.00001)
						new_address = int(self.Handshake['Value'].value[0]) - self.firmware.base_address
						self.currentDataOffset = new_address
						self.stallRequested = False

				elif self.Handshake['Command'].value[0] == enumerator_by_name('AbortTransaction', self.CommandEnum):
					print('\nBootloader aborted transaction!', file=self.output_file)
					sys.exit(1) #todo this deserves a bit nicer handling

	def receive_generic_response(self, id, sent : tuple, must_match : list, wanted : list):
		message = self.db.getMsgById(id)

		for field in must_match:
			expected = sent._asdict()[field]
			received = message[field].value[0]
			if expected != received:
				print(f'Received unrelated acknowledge! Field {field} does not match ({expected=}, {received=})', file=sys.stderr)
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
		entry_time = time.time()
		last_print = entry_time
		while True:
			ev = self.get_next_message()

			if time.time() - last_print > 10:
				print(f'Awaited response 0x{expected_id:03x} was not received for {time.time() - entry_time:.2f} seconds.', file=self.output_file)
				last_print = time.time()

			if ev.id.value in (self.SoftwareBuild.identifier, self.Beacon.identifier):
				continue #these messages are not part of protocol - ignore them

			if ev.id.value != expected_id:
				continue

			if ev.id.value == self.HandshakeAck.identifier:
				return self.receive_generic_response(self.HandshakeAck.identifier, sent, ['Register', 'Value', 'Target'], ['Response'])
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
				# PingResponses may clash a lot, because this message is used to detect presence as well as request bootloader.
				# Therefore it is necessary to filter incomming messages. It is safe to ignore PingResponses from other units
				# as this message does not carry mission critical information.
				message = self.db.getMsgById(self.PingResponse.identifier)

				from_target = sent.Target == message['Target'].value[0]
				if not from_target:
					continue
				return [message['BootloaderPending'].value[0]]


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
		return self.send_generic_message_and_await_response(self.Handshake.identifier, [r, c, self.target, value], self.HandshakeAck.identifier)[0]

	def send_data(self, address, word):
		return self.send_generic_message_and_await_response(self.Data.identifier, [address, False, word], None, False)

	def send_handshake_ack(self, reg, response, value):
		return self.send_generic_message_and_await_response(self.HandshakeAck.identifier, [reg, self.target, response, value], None, False)

	def yield_communication(self):
		self.isBusMaster = False
		return self.send_generic_message_and_await_response(self.CommunicationYield.identifier, [self.target], None, False)

	def send_transaction_magic(self):
		return self.send_handshake("TransactionMagic", "None", FlashMaster.transactionMagic)

	def send_command(self, command, value = 0):
		return self.send_handshake("Command", command, value)


	def request_bootloader_exit(self, target, force, toApp):
		return self.send_generic_message_and_await_response(self.ExitReq.identifier, [target, force, toApp], self.ExitAck.identifier)[0]

	def request_bootloader_entry(self, target):
		return self.send_generic_message_and_await_response(self.Ping.identifier, [target, True], self.PingResponse.identifier)[0]

	def print_header(self):
		print('Desktop interface to CAN Bootloader', file=self.output_file)
		print('Written by Vojtech Michal, (c) eForce FEE Prague Formula 2020, 2021\n', file=self.output_file)

		if not args.verbose:
			return

		targets = [elem.name for val, elem in self.BootTargetEnum.enum.items()]
		if len(targets) == 1:
			print(f"The only targetable unit is {targets[0]}", file=self.output_file)
		else:
			print(f"Targetable units are {{{', '.join(targets)}}}", file=self.output_file)

	def parseFirmware(self, firmwarePath, parsing_bootloader):
		if args.verbose:
			print(f'Parsing hex file {firmwarePath}', file=self.output_file)
		self.firmware = Firmware(firmwarePath)
		if args.verbose:
			print(f'Input consists of {len(self.firmware.records)} hex records.', file=self.output_file)

		print(f'{"New bootloader" if parsing_bootloader else "Firmware"} logical memory map ({self.firmware.length} B in total, {len(self.firmware.flattened_map)} B flattened):', file=self.output_file)
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
		SeqError = enumerator_by_name('HandshakeSequenceError', self.HandshakeResponseEnum)

		while True:
			self.get_next_message(self.Handshake.identifier)
			reg, command, target, value = map(lambda f: f.value[0], self.Handshake.fields)

			if target != self.target:
				print(f'Ignoring handshake from {target}.')
				continue

			if reg != register:
				self.send_handshake_ack(reg, SeqError, value)
				continue

			self.send_handshake_ack(reg, enumerator_by_name('OK', self.HandshakeResponseEnum), value)
			return value

	def receive_transaction_magic(self):
		while True:
			self.get_next_message(self.Handshake.identifier)
			reg, command, target, value = map(lambda f: f.value[0], self.Handshake.fields)

			if target != self.target:
				print(f'Ignoring transaction magic from {target}.')
				continue

			res = self.checkMagic(reg, command, value)
			self.send_handshake_ack(reg, res, value)
			if res == enumerator_by_name('OK', self.HandshakeResponseEnum):
				return #transaction magic is ok, we can go further

	def reset_BL_to_application(self, force):
		is_bootloader_active = lambda target: target in self.listing.active_bootloaders
		is_application_active = lambda target: target in self.listing.aware_applications and self.listing.aware_applications[target].last_response is not None
		self.ocarinaReadingThread.start()

		print(f'Searching for {self.targetName} (target ID {self.target}) on the bus.\n', file=self.output_file)
		#wait for the target unit to show up
		while not is_bootloader_active(self.target) and not is_application_active(self.target):
			time.sleep(0.01)

		if is_application_active(self.target):
			print('The target unit is already in application. Nothing to do.', file=self.output_file)
			return

		#the bootloader is active and responding. Request it to reset to the application

		print('Leaving bootloader... ', end='', file=self.output_file)

		result = self.request_bootloader_exit(self.target, force = force, toApp = True)
		print('Confirmed' if result else 'Declined', file=self.output_file)


	def estabilish_connection_to_slave(self, force):
		self.ocarinaReadingThread.start()

		print('Searching for bootloader aware units present on the bus...', file=self.output_file)

		is_bootloader_active = lambda target: target in self.listing.active_bootloaders
		is_application_active = lambda target: target in self.listing.aware_applications and self.listing.aware_applications[target].last_response is not None

		#wait for the target unit to show up
		while not is_bootloader_active(self.target) and not is_application_active(self.target):
			self.send_generic_message_and_await_response(self.Ping.identifier, [self.target, True], 0, False)
			time.sleep(0.2)

		if args.verbose:
			print("Target's presence on the CAN bus confirmed.", file=self.output_file)

		#the target has application running -> request it to reset into bootloader
		if is_application_active(self.target):
			print(f'Waiting for {self.targetName} to enter the bootloader.', file=self.output_file)

			for i in range(5):
				if self.request_bootloader_entry(self.target):
					break
			else:
				print('Target unit refused to enter the bootloader. Exiting', file=self.output_file)
				return
			#await the bootloader's activation
			while not is_bootloader_active(self.target): 
				time.sleep(0.01)

		elif is_bootloader_active(self.target):
			if args.verbose:
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
				if not self.request_bootloader_exit(self.target, force = True, toApp = False):
					print('Bootloader refused to abort ongoing transaction.', file=self.output_file)
					return
				#wait for the bootloader to become ready
				while self.listing.active_bootloaders[self.target].state != ready_state:
					time.sleep(0.1)
		else:
			assert False

		print('Target bootloader is ready.', file=self.output_file)

	def send_initial_transaction_magic(self):
		if args.verbose:
			print('\nTransaction starts\n', file=self.output_file)
			print('Sending initial transaction magic ... ', end='', file=self.output_file)

		self.report_handshake_response(self.send_transaction_magic())

	def set_vector_table(self, address, force):
		if args.verbose:
			print(f"Initiating flash process for {self.targetName} (target ID {self.target})\n", file=self.output_file)

		self.estabilish_connection_to_slave(force)
		self.send_initial_transaction_magic()

		#actually send the new value
		print(f'Setting address 0x{address:08x} as the new interrupt vector table.', file=self.output_file)
		self.report_handshake_response(self.send_command('SetNewVectorTable', address))

		#some more stupid messages to show that we are doing work.
		if args.verbose:
			print('Interrupt vector table address updated sucessfully.', file=self.output_file)
		print('Leaving bootloader... ', end='', file=self.output_file)

		result = self.request_bootloader_exit(self.target, force = False, toApp = True)
		print('Confirmed' if result else 'Declined', file=self.output_file)

	def flash(self, update_bootloader, force):
		if args.verbose:
			print(f"Initiating {'bootloader update' if update_bootloader else 'flash process'} for {self.targetName} (target ID {self.target})\n", file=self.output_file)

		self.estabilish_connection_to_slave(force)
		# make the listing ping slowly from now. The connection has been established and hence we're not in hurry anymore
		self.listing.pingCyclePeriod = self.listing.pingCyclePeriodSlow
		self.send_initial_transaction_magic()

		if args.verbose:
			print('Initiating flashing transaction ... ' if not update_bootloader else 'Initiating bootloader update ... ', end = '', file=self.output_file)
		self.report_handshake_response(self.send_command('StartBootloaderUpdate' if update_bootloader else 'StartTransactionFlashing'))
		
		if args.verbose:
			print('Communication yields...', file=self.output_file)
		self.yield_communication()

		#receive initial transaction magic
		self.receive_transaction_magic()
		if args.verbose:
			print('Bootloader sent initial magic.', file=self.output_file)
		#receive the number of physical memory blocks
		physical_memory_map = [None] * int(self.receive_handshake('NumPhysicalMemoryBlocks'))
		
		if args.verbose:
			print(f'Available memory consists of {len(physical_memory_map)} blocks. ', file=self.output_file)

		for i in range(len(physical_memory_map)): #receiving blocks
			start = int(self.receive_handshake('PhysicalBlockStart'))
			length = int(self.receive_handshake('PhysicalBlockLength'))

			physical_memory_map[i] = MemoryBlock(start, 'x' * length)

		block_smallest = min(map(lambda block: len(block.data), physical_memory_map))
		block_biggest = max(map(lambda block: len(block.data), physical_memory_map))

		if block_smallest == block_biggest: #all blocks have the same size
			memory_map_str = f"{len(physical_memory_map)} blocks {len(physical_memory_map[0].data)} bytes long from {physical_memory_map[0].address:08x}."
		else: #the blocks are not equally long
			memory_map_str = ''
			for index in range(0, len(physical_memory_map), memory_map_print_elems_per_line):
				memory_map_str += f'\n\t{index:3}:'
				memory_map_str +=''.join(map(lambda block: f" [0x{block.address:08x}-0x{block.address + len(block.data)-1:08x}]", physical_memory_map[index:index+memory_map_print_elems_per_line]))
		print(f'Available memory map: {memory_map_str}')

		#receive terminal transaction magic
		self.receive_transaction_magic()

		if args.verbose:
			print('Awaiting bootloader yield ... ', end= '', file=self.output_file)
		self.get_next_message(self.CommunicationYield.identifier)
		if args.verbose:
			print('OK\n\n', file=self.output_file)

		used_physical_memory = self.firmware.identify_influenced_physical_blocks(physical_memory_map)
		if used_physical_memory is None:
			print('Available physical memory cannot cover all address ranges required by firmware! Exiting.', file=self.output_file)
			return
		else:
			print(f'Will use {len(used_physical_memory)} physical pages in total to fit the {"new bootloader" if update_bootloader else "firmware"}. That is')
			start_address = used_physical_memory[0].address
			consecutive_block_start = 0
			for index, (prev, this) in enumerate(zip(used_physical_memory[:-1], used_physical_memory[1:]), 1):
				if this.address != prev.address + len(prev.data): #we have found a hole
					print(f'\t{index - consecutive_block_start} pages in range [0x{start_address:08x}, {prev.address + len(prev.data):08x}] ({(prev.address + len(prev.data) - start_address)/1024:.2f} KiB)')
					start_address = this.address
					consecutive_block_start = index

			last_block = used_physical_memory[-1]
			print(f'\t{len(used_physical_memory) - consecutive_block_start} pages in range [0x{start_address:08x}, 0x{last_block.address + len(last_block.data):08x}] ({(last_block.address + len(last_block.data) - start_address)/1024:.2f} KiB)')

		##Transmission of logical memory map

		if args.verbose:
			print('Sending initial magic ... ', end='', file=self.output_file)
		self.report_handshake_response(self.send_transaction_magic()) #initial magic

		if args.verbose:
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
			print(f'Sending {len(used_physical_memory)} page addresses ... ', file=self.output_file)
		self.report_handshake_response(self.send_transaction_magic()) #initial magic
		self.report_handshake_response(self.send_handshake('NumPhysicalBlocksToErase', 'None', len(used_physical_memory)))
		erassureStart = time.time()
		for index, page in enumerate(used_physical_memory, 1):
			print(f'\r\tErasing page {index:2}/{len(used_physical_memory):2} @ 0x{page.address:08x} ... ', end = '', file=self.output_file)
			result = self.send_handshake('PhysicalBlockToErase', "None", page.address)
			if result != enumerator_by_name('OK', self.HandshakeResponseEnum):
				print(f"Page {index} erassure returned result {self.HandshakeResponseEnum.enum[result].name}", file = sys.stderr)
		print('OK', file=self.output_file)
		print(f'Flash erassure took {(time.time() - erassureStart)*1000:5.2f} ms')
		self.report_handshake_response(self.send_transaction_magic()) #terminal magic

		##Firmware download

		while self.busBitrate is None:
			print('Cannot determine bus bitrate', file= sys.stderr)
			time.sleep(0.5)
		print(f'Bus bitrate {self.busBitrate} kbps')

		if args.verbose:
			print('Sending initial magic ... ', end = '', file=self.output_file)
		self.report_handshake_response(self.send_transaction_magic())


		if args.verbose:
			print(f'Sending firmware size ({self.firmware.length} B)', end = '... ' if args.verbose else '\n', file=self.output_file)
		self.report_handshake_response(self.send_handshake('FirmwareSize', 'None', self.firmware.length))

		print(f'Sending {self.firmware.length/1024:.2f} KiB of {"new bootloader" if update_bootloader else "firmware"}...', file=self.output_file)
		print(f'\tProgress ... {0:05}%', end='', file=self.output_file)
		start = time.time()
		last_print = time.time()

		if args.quiet:
			self.listing.pause()

		totalSentBytes = 0
		self.currentDataOffset = 0
		self.dataTransmissionInProgress = True

		while not self.dataTransmissionFinished:
			if self.currentDataOffset == len(self.firmware.flattened_map):
				time.sleep(0.1)
				continue

			while self.stallRequested:
				time.sleep(0.001)


			absolute_address = self.currentDataOffset + self.firmware.base_address
			data = self.firmware.flattened_map[self.currentDataOffset: self.currentDataOffset + 4]
			assert all(byte is not None for byte in data) #all messages will carry whole word
			data_as_word = sum(byte << index*8 for index, byte in enumerate(data))
			self.send_data(absolute_address, data_as_word)

			totalSentBytes += 4
			self.currentDataOffset += 4

			if self.currentDataOffset != len(self.firmware.flattened_map) and self.firmware.flattened_map[self.currentDataOffset] is None: #we have hit an empty space between two logical memory blocks
				previous_block = list(filter(lambda block: block.address + len(block.data) == self.currentDataOffset + self.firmware.base_address, self.firmware.logical_memory_map))
				assert len(previous_block) == 1# only a single block may has ended here
				next_block = self.firmware.logical_memory_map[self.firmware.logical_memory_map.index(previous_block[0])]
				new_data_offset = next_block.address - self.firmware.base_address
				assert all(byte is None for byte in self.firmware.flattened_map[self.currentDataOffset : new_data_offset])

				self.currentDataOffset = new_data_offset

			if time.time() - last_print > 0.01:
				print(f'\r\tProgress ... {100 * self.currentDataOffset / len(self.firmware.flattened_map):6.2f}% (avg {self.currentDataOffset/1024/(time.time()-start):5.2f} KiBps, efficiency {100*self.currentDataOffset/totalSentBytes:6.2f}%, {1000*self.totalTimeStalled:5.2f} ms stalled)', end='', file=self.output_file)
				last_print = time.time()
			if self.currentDataOffset/totalSentBytes < 0.9: #introduce a delay if the BL has problems catching up
				time.sleep(0.00023)
		self.dataTransmissionInProgress = False
		dataEfficiency = self.currentDataOffset/totalSentBytes

		print(f'\r\tProgress ... {100:5.2f}% (avg {self.firmware.length/1024/(time.time()-start):2.2f} KiBps, efficiency {100*dataEfficiency:3.3f}%, {1000*self.totalTimeStalled:5.2f} ms stalled))           ', file=self.output_file)
		duration = (time.time() - start)
		print(f'Took {duration*1000:.2f} ms, stalled {100*self.totalTimeStalled/duration:4.2f}% of time.')

		checksum = self.firmware.calculate_checksum()

		if args.verbose:
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
			print(f'Sending interrupt vector 0x{self.firmware.logical_memory_map[0].address:08x}... ', end = '', file=self.output_file)
		#TODO make sure this is really the interrupt vector
		self.report_handshake_response(self.send_handshake('InterruptVector', "None", self.firmware.logical_memory_map[0].address))

		if args.verbose:
			print(f'Sending entry point address 0x{self.firmware.entry_point:08x} ... ', end = '', file=self.output_file)
		self.report_handshake_response(self.send_handshake('EntryPoint', "None", self.firmware.entry_point))

		if args.verbose:
			print('Sending terminal transaction magic ... ', end = '', file=self.output_file)
		self.report_handshake_response(self.send_transaction_magic())

		if args.verbose:
			print('Firmware flashed successfully', file=self.output_file)
		print('Leaving bootloader... ', end='', file=self.output_file)

		result = self.request_bootloader_exit(self.target, force = False, toApp = True)
		print('Confirmed' if result else 'Declined', file=self.output_file)

		print('\n')
		if self.totalTimeStalled:
			print('NOTE: Stalls occured during the transaction, which may indicate that BL clock is too slow.')
			print('Increasing BL clock frequency is suggested.')
		if dataEfficiency < 1:
			print(f'NOTE: Data transmission has been only {dataEfficiency*100:5.2f}% efficient due to low bus baudrate or high communication traffic.')
			print('Suggested selecting a different bus with higher possible throughput.')
		self.terminate()

	def terminate(self):
		self.exit = True
		self.ocarinaReadingThread.join()
		


if args.feature == 'list':
	if args.terminal is None: #TODO allow non-interactive runs (stdin may not be bound to any tty)
		args.terminal = os.ttyname(sys.stdout.fileno())
	listing = BootloaderListing(oc, db, args.terminal)

	while True:
		ev = oc.read_event(blocking = True)
		assert ev is not None
		listing.process_event(ev)

	listing.terminate()

elif args.feature == 'flash' or args.feature == 'update_bootloader':

	if args.unit is None:
		print('You must specify the unit to flash using "-u name"')
		sys.exit(1)

	if args.firmware is None:
		print('You must specify the binary to flash using "-x path"')
		sys.exit(1)

	update_bootloader = args.feature == 'update_bootloader'
	master = FlashMaster(args.unit, args.terminal, args.quiet)
	master.parseFirmware(args.firmware, update_bootloader)
	master.flash(update_bootloader, args.force)

elif args.feature == 'set_vector_table':

	assert args.unit is not None
	assert args.address is not None
	args.address = int(args.address, 16 if 'x' in args.address or 'X' in args.address else 10)
	master = FlashMaster(args.unit, args.terminal, args.quiet)
	master.set_vector_table(args.address, args.force)

elif args.feature == 'enter':
	master = FlashMaster(args.unit, args.terminal, args.quiet)
	master.estabilish_connection_to_slave(args.force)

elif args.feature == 'exit':
	master = FlashMaster(args.unit, args.terminal, args.quiet)

	master.reset_BL_to_application(args.force)

else:
	assert False

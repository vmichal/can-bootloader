#!/usr/bin/env python3

import os
import struct
import argparse
import sys

parser = argparse.ArgumentParser(description='Desktop interface to CAN bootloaders embedded into the electric formula.',\
	formatter_class=argparse.ArgumentDefaultsHelpFormatter)

parser.add_argument('-j', metavar="file", dest='json_files', type=str, action='append', help='add candb json file to parse')
parser.add_argument('-f', dest='feature', type=str, choices=["list", "flash"], default="list", help='choose feature - list bootloader aware units or flash new firmware')
parser.add_argument('-x', dest='firmware', type=str, help='Path to hex file for flashing')

parser.add_argument('ocarina',  metavar='ocarina', type=str, help='path to ocarina COM port device (/dev/ocarina, /dev/ttyUSB0,...)')

args = parser.parse_args()

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

printtime = time.time()
id_seen = [] #list all of msg ids received during this session

def find_bootloader_beacon(): #returns the obect with message BootloaderBeacon
	for id, msg in db.parsed_messages.items():
		if msg.owner == 'Bootloader' and msg.description.name == 'BootloaderBeacon':
			return msg

def list_bootloader_aware_units():
	beacon = find_bootloader_beacon() #all bootloader aware units transmit this message
	fields = beacon.fields

	try:
		target_index = fields.index(next(field for field in fields if field.description.name == "Unit"))
		state_index  = fields.index(next(field for field in fields if field.description.name == "State"))
		flash_index  = fields.index(next(field for field in fields if field.description.name == "FlashSize"))
	except:
		print("ERROR: Given message does not include fields 'Unit', 'State', 'FlashSize'", file=sys.stderr)
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



		


clearscreen()
if args.feature == 'list':
	list_bootloader_aware_units();
elif args.feature == 'flash':
	flash

else:
	assert False


#Sniffs incomming data for serial output and prints it to terminal
def sniff_serial_output(message_id : int):
	fields = db.getMsgById(message_id).fields

	try:
		seq_index = fields.index(next(field for field in fields if field.description.name == "SEQ"))
		truncated_index = fields.index(next(field for field in fields if field.description.name == "Truncated"))
		completed_index = fields.index(next(field for field in fields if field.description.name == "Completed"))
		payload_index = fields.index(next(field for field in fields if field.description.name == "Payload"))
	except:
		print("ERROR: Given message does not include fields 'SEQ', 'Truncated', 'Completed', 'Payload'", file=sys.stderr)
		return

	expectedSEQ = 0
	while True:
		#receive message and check whether we are interested
		ev = oc.read_event()
		if ev == None or not isinstance(ev, ocarina.CanMsgEvent) or ev.id.value != message_id:
			continue

		#extract information from message's bits
		db.parseData(message_id, ev.data, ev.timestamp)
		fields = db.getMsgById(message_id).fields

		seq = fields[seq_index] # sequence counter, increasing number between 0 and (typically) 15
		truncated = fields[truncated_index] 
		completed = fields[completed_index] #Board sent all enqued data
		payload = fields[payload_index] # 7 characters of output

		if int(seq.value[0]) != expectedSEQ:
			print(f"\aCommunication packet {expectedSEQ} missing, received {int(seq.value[0])} instead.", file=sys.stderr)
		expectedSEQ = (int(seq.value[0]) + 1) % int(seq.vmax+1)

		if truncated.value[0]:
			print("\aStdout buffer overflow.", file=sys.stderr)

		sys.stdout.write("".join(map(chr, map(int, filter(lambda c: c != 0, payload.value)))))
		if completed.value[0]:
			sys.stdout.flush()

if args.feature == "stdout":
	clearscreen()
	if id_filter == None or len(id_filter) != 1:
		print("ERROR: For stdout sniffing, a single message id must be specified using -i.", file=sys.stderr)
		sys.exit()

	searched_id = id_filter[0]

	if not db.isMsgKnown(searched_id):
		print(f"ERROR: Message {searched_id} is not recognized in this JSON.", file=sys.stderr)
		sys.exit()


	sniff_serial_output(searched_id)
	sys.exit()

while(True):
	ev = oc.read_event()
	t = time.time()
	if isinstance(ev, ocarina.CanMsgEvent):
		if ev != None and db.isMsgKnown(ev.id.value) and (id_filter == None or ev.id.value in id_filter) :
			db.parseData(ev.id.value, ev.data, ev.timestamp)
			if ev.id.value not in id_seen:
				id_seen.append(ev.id.value)
			
	if args.feature == "acpmon7":
		# 	print("ACP MON!")
		if t >= printtime:
			clearscreen()
			printtime = t + args.refreshtime
			msg = db.getMsgById(0x496)
			print(msg.description.name)
			print("tdelta {:5.1f} ms".format(msg.getTSdiff()*1000))
			stackCnt = int(msg.fields[0].vmax)+1
			setCnt = int(msg.fields[0].muxedFields[0][0].vmax)
			cellCnt = int(msg.fields[0].muxedFields[0][0].muxedFields[0][1].count)
			acpCells = [[None for _ in range(setCnt * cellCnt)] for _ in range(stackCnt)]
			for setid in  range(setCnt):
				for cell in range(cellCnt):
					for stackid in range(stackCnt):
						# print("{:4.0f}  ".format(msg.fields[0].muxedFields[stackid][0].muxedFields[setid][1].value[cell]), end = "")
						acpCells[stackid][setid*cellCnt + cell] =  msg.fields[0].muxedFields[stackid][0].muxedFields[setid][1].value[cell]
					# print("")
			
			for stack in range(len(acpCells)):
				print(f"Stack {stack:1d} |",  end="")
				for cell in acpCells[stack]:
					if cell == None or cell == 0:
						print(f"     ", end="")
					else:
						print(f" {cell:4.0f}", end="")
					# if cnt == int(len(stack)/2):
					# 	print("")
				print("")
			
			
			print("")
			# print(msg)
			# exit(0)
			msg = db.getMsgById(0x497)
			print(msg.description.name)
			print("tdelta {:5.1f} ms".format(msg.getTSdiff()*1000))
			stackCnt = int(msg.fields[0].vmax)+1
			setCnt = int(msg.fields[0].muxedFields[0][0].vmax)
			tempCnt = int(msg.fields[0].muxedFields[0][0].muxedFields[0][0].count)
			acpTemps = [[None for _ in range(setCnt * tempCnt)] for _ in range(stackCnt)]
			for stackid in range(stackCnt):
				for setid in range(setCnt):
					for temp in range(tempCnt):
						# print("{:3.0f} ".format(msg.fields[0].muxedFields[stackid][0].muxedFields[setid][0].value[temp]), end = "")
						acpTemps[stackid][setid*tempCnt + temp] =  msg.fields[0].muxedFields[stackid][0].muxedFields[setid][0].value[temp]
				# print("")
			
			for stack in range(len(acpTemps)):
				print(f"Stack {stack:1d}.A |", end="")
				cnt = 0
				for temp in acpTemps[stack]:
					if temp == None or temp < 0:
						print(f"    ", end="")
					else:
						print(f" {temp:3.0f}", end="")
					cnt += 1
					if cnt == int(len(acpTemps[stack])/2):
						print(f"\nStack {stack:1d}.B |", end="")
				print("")
			
			tempMax =  max([max(stack) for stack in acpTemps])
			print(f"ACP temp MAX: {tempMax}")
			print("Stack MAX: ",end="")
			for stack in range(len(acpTemps)):
				stackMax = max(acpTemps[stack])
				print(f"S{stack:1d}: {stackMax:2.0f} |", end="")
			print("")

			

			# print(msg)
					
	
	elif args.feature == "sniff":
		if t >= printtime:
			clearscreen()
			print("rxed IDs")
			printtime = t + 0.1
			for i in id_seen:
				m = db.getMsgById(i)
				print("delta {:1.6f}, msg: ".format(m.getTSdiff()), end = "")
				print(m)
			# oc.send_message_std(176, )

	# else:
	# 	exit(1)

#!/usr/bin/env python3

"""
Classes which represent canDB messages, fields, enums etc. Parsed from json file.
They parse values from packets if raw data given.
"""
import traceback
from os import system, name 
import traceback
import copy
import json
import math

from enum import Enum
from collections import OrderedDict

def validate_not_empty(src, fallback):
	if (src==None):
		return fallback
	elif (isinstance(src, str) and not src):
		return fallback
	else:
		return src

def toFloat(s, fallback):
	s = validate_not_empty(s, fallback)
	if isinstance(s, int) or isinstance(s, float):
		return float(s)

	if "0x" in s:
		return float(int(s, 16))#can handle strings line 0xAA and such
	else:
		return float(s)


def extractBits(numList, offset , size):
	numList = list(numList)
	# print(numList)
	data = 0
	if (len(numList) == 1):
		data = numList[0]
	else:
		for b in range(len(numList)):
			data = data + (numList[b]<<(8*b))
		data = (data >> offset)
	
	data = data & ((1<<size)-1)
	
	return data

def toSignedNumber(number, bitLength):
	mask = (1 << bitLength) - 1
	if number & ~(mask >> 1):
		return number | ~mask
	else:
		return number & mask

def toUnsignedNumber(number, bitLength):
	if not isinstance(number, int):
		raise Exception("number must be integer, was {}".format(type(number)))

	if number >= 0:
		return  number

	if -number > (1 << bitLength)/2:
		raise Exception("number {} does not fit into {} bits".format(number, bitLength))

	mask = (1 << bitLength) - 1
	return mask ^ (-number -1)


def extractUnsignedInt(dataList, offset, size):
	return extractBits(dataList, offset, size)

def extractSignedInt(dataList, offset, size):
	return toSignedNumber(extractBits(dataList, offset, size), size)

def insertBits(dataU8List, integer, offset, size):
	if not isinstance(integer, int):
		raise Exception("integer must be instance of int, was {}".format(type(integer)))

	if integer < 0:
		integer = toUnsignedNumber(integer, size)
	
	if integer > (1<<size) - 1:
		raise Exception("bitsize {} too small for number {}".format(size, integer))

	if offset + size > len(dataU8List)*8:
		raise Exception("buffer too small, required {}, available {}".format(offset + size, len(dataU8List)*8))
	
	byteOffset = int(offset / 8)
	subByteOffset = offset % 8

	# split number to list of 8bit chunks (necessary for conversion to bytes)
	splitBits = []
	integer << subByteOffset
	while integer > 0:
		splitBits.append(integer&0xFF)
		integer >>= 8
	
	for b in splitBits:
		dataU8List[byteOffset] |= b
		byteOffset += 1

class Custom_enum_element:
	def __init__(self, name, value=None, description_text=None):
		self.value=value
		self.name=name
		self.description_text=description_text

class Custom_enum():
	def __init__(self, name):
		# print("NEW ENUM "+name)
		self.name = name
		self.enum = OrderedDict()
		self.type = "candb_enum_{}_t".format(self.name)
	
	def append(self, element, implicitIsFine = False):
		if (not isinstance(element, Custom_enum_element)):
			raise Exception("Enum element is not instance of Custom_enum_element, but: {}".format(type(element)))
		if (validate_not_empty(element.name, None) == None):
			print("\tempty enum key, ignoring")
			return
		if (element.value in self.enum):
			raise Exception("Enum already has value {} ,called {}".format(element.value, element.name))

		if (element.value==None):
			# print("WARNING: implicit value in enum!")
			if not implicitIsFine:
				print("WARNING: implicit value in enum!")
			if (len(self.enum)==0):#if empty
				element.value = 0#start with zero
			else:	
				element.value = max(self.enum)+1 #othwerwise take previous value and add 1
		self.enum[element.value] = element
	
	def min(self):
		return self.enum[min(self.enum.keys())]
	
	def max(self):
		return self.enum[max(self.enum.keys())]

class Description():
	def __init__(self, name, text):
		self.name = name
		if text != None:
			self.text = text.replace("\n","").replace("\r","")
		else:
			self.text = ""
	
	def __str__(self):
		return "{} | {}".format(self.name, self.text)

class Candb_msg_field_type(Enum):
	BOOL=0
	UINT=1
	INT=2
	FLOAT=3
	ENUM=4
	MUX=5

	def __str__(self):
		return '{0}'.format(self.name)

	@staticmethod
	def from_str(s):
		if ("uint" in s):
			return Candb_msg_field_type.UINT
		elif ("int" in s):
			return Candb_msg_field_type.INT
		elif ("bool" in s):
			return Candb_msg_field_type.BOOL
		elif ("multiplex" in s):
			return Candb_msg_field_type.MUX
		elif ("enum" in s):
			return Candb_msg_field_type.ENUM
		elif ("float" in s):
			return Candb_msg_field_type.FLOAT
		else:
			raise Exception("Unknown field type: "+s)

class Candb_msg_field():
	def __init__(self, description, field_type, count, bits, pos_offset, unit, available_enums, vmin=0, vmax=0, voffset=0, vfactor=1):
		if (not isinstance(description, Description)):
			raise Exception("description must be instance of class Description, is:", type(description))

		self.description = description
		self.field_type = Candb_msg_field_type.from_str(field_type)
		self.field_type_raw = field_type
		self.nestLevel = 0#used for indent calculation in __Str__


		self.bits = bits
		self.pos_offset = pos_offset
		self.count = count
		self.unit = unit
		self.vmin = toFloat(vmin, -math.inf)
		self.vmax = toFloat(vmax, math.inf)
		self.voffset = toFloat(voffset, 0)
		self.vfactor = toFloat(vfactor, 1)
		if (self.vfactor == 0):
			print("error, field '{}' has factor 0, resetting to 1".format(self.description))
			self.vfactor = 1
		defvalue = self.vmin

		

		if (self.field_type == Candb_msg_field_type.MUX):
			if (self.count != 1):
				raise Exception("Mux and array at the same time is not suported")
			else:
				self.muxedFields = [ [] for i in range(int(self.vmax - self.vmin) + 1) ]
		else:
			self.muxedFields = None # used in muxed type field

			if (self.field_type == Candb_msg_field_type.UINT):
				if self.vmin < 0:
					defvalue = 0 #because uints cant go below 0...
	
			elif (self.field_type == Candb_msg_field_type.INT):
				#signed values probably have idle value on 0 and not on min
				if (0 > self.vmin  and 0 < self.vmax):
					defvalue = 0
		
			elif (self.field_type == Candb_msg_field_type.ENUM):
				#link corresponding enum to this field
				enum_name = self.field_type_raw.split(" ")[1]#eg "enum ECUF_CAL_STWIndex" > ECUF_CAL_STWIndex
				self.linked_enum = None
				for e in available_enums:
					if e.name == enum_name:
						self.linked_enum = e
				if (self.linked_enum == None):
					raise Exception("matching enum not found ({})".format(enum_name))
				#set correct min/max
				self.vmin = self.linked_enum.min().value
				self.vmax = self.linked_enum.max().value
				defvalue = self.linked_enum.min().value
			
			elif (self.field_type == Candb_msg_field_type.BOOL):
				self.vmin = False
				self.vmax = True
				defvalue = self.vmin
		
		self.value = list([defvalue]*self.count)

		
	
	def __str__(self):
		indent = "    "*(self.nestLevel+1)
		v = ""
		for idx in range(self.count):
			if self.field_type in [Candb_msg_field_type.INT, Candb_msg_field_type.UINT, Candb_msg_field_type.BOOL]:
				v += "{:7.{prec}f} ".format(self.value[idx], prec = int(math.log10(max([int(1/self.vfactor), 1]))) )
			elif self.field_type is Candb_msg_field_type.FLOAT:
				v += "{:7.3f} ".format(float(self.value[idx]))
			elif self.field_type is Candb_msg_field_type.ENUM:
				v += "{} (={:2d})".format(self.linked_enum.enum[self.value[idx]].name, self.linked_enum.enum[self.value[idx]].value)
			elif self.field_type is Candb_msg_field_type.MUX:
				v += "{:7d} ".format(int(self.value[idx]))
			else:
				v = "N/A "

		#eg: UINT   (56:52)    [ 4]  = 0  SEQ | Message up counter for safety
		v = indent + "{:5} ({:2}:{:2} = {:2}) [{:1}] = {} {}".format(self.field_type.name, self.pos_offset+self.bits, self.pos_offset, self.bits, self.count, v, self.description)
		
		#FIXME indentation works only for 1 level
		if self.field_type is Candb_msg_field_type.MUX:
			v += "\n"
			for m in range(int(self.vmax - self.vmin) + 1):
				v += indent + "{}[{}]\n".format(self.description.name, m)
				for sub in self.muxedFields[m]:
						v += "{}\n".format(sub)
			else:
				pass
		return v
	
	def addMuxedSubfield(self, field):
		if self.field_type != Candb_msg_field_type.MUX:
			raise Exception("this field is not MUX type, but {}".format(type(field)))
		for sub in self.muxedFields:
			if len(sub) > 0 and sub[-1].isMux():
				#nested muxes, forward vield to submux
				sub[-1].addMuxedSubfield(field)
			else:
				sub.append(copy.deepcopy(field))
				sub[-1].nestLevel = self.nestLevel+1
	
	def isMux(self):
		return self.field_type == Candb_msg_field_type.MUX
	
	def isArray(self):
		return self.count > 1
	
	def parseFromPacket(self, dataList):
		for arr in range(self.count):
			if self.field_type in [Candb_msg_field_type.UINT, Candb_msg_field_type.BOOL, Candb_msg_field_type.MUX]:
				raw = extractUnsignedInt(dataList, self.pos_offset + arr*self.bits, self.bits)
			elif (self.field_type == Candb_msg_field_type.INT):
				raw = extractSignedInt(dataList, self.pos_offset + arr*self.bits, self.bits)
			elif (self.field_type == Candb_msg_field_type.ENUM):
				raw = self.linked_enum.enum[extractUnsignedInt(dataList, self.pos_offset + arr*self.bits, self.bits)].value#translate number to enum element (value, name)
			else:
				raise Exception("paring of type '{}' not yet supported".format(self.field_type))
			
			if self.field_type in [Candb_msg_field_type.UINT, Candb_msg_field_type.INT]:
				raw *= self.vfactor
				raw += self.voffset
			
			
			self.value[arr] = raw

			if raw > self.vmax:
				print("WARNING: {} value above allowed range ({} > {})".format(self.description.name, raw, self.vmax))
			elif raw < self.vmin:
				print("WARNING: {} value below allowed range ({} < {})".format(self.description.name, raw, self.vmin))
			
		
		if self.isMux():
			#forward data parsing to muxed subfields
			index = int(self.value[0]-self.vmin)
			if index >= 0 and index < len(self.muxedFields):
				for f in self.muxedFields[index]:
					f.parseFromPacket(dataList)
			else:
				print("WARNING: ignored MUX parsing, index out of range!")
	
	def assemble(self, values, buff):
		value = None

		if self.field_type == Candb_msg_field_type.MUX:
			print("MUXfield", self.description.name, self.description.text, self.field_type, self.field_type_raw)
			value = int(values.pop(0))
			muxBranch = int(value - self.vmin)
			if muxBranch >= 0 and muxBranch < len(self.muxedFields):
				for f in self.muxedFields[muxBranch]:
					values = f.assemble(values, buff)
			else:
				print("WARNING: ignored MUX parsing, index out of range!")

		elif self.field_type in [Candb_msg_field_type.UINT, Candb_msg_field_type.BOOL, Candb_msg_field_type.MUX, Candb_msg_field_type.INT, Candb_msg_field_type.ENUM]:
			for _ in range(self.count):
				# try:
				value = int(values.pop(0))
				insertBits(buff, int((value - self.voffset) / self.vfactor), self.pos_offset, self.bits)
				# except Exception as e:
				# 	print("failed bit insertion, index {}".format(i), e)
		else:
			raise Exception("this field type assembly not implemented, type {}".format(self.field_type))
		
		return values

class Candb_msg():
	def __init__(self, description, owner, sent_by, identifier, length, busname, timeout, period, ):
		if (not isinstance(description, Description)):
			raise Exception("description must be instance of class Description")
		self.description = description
		self.sent_by = sent_by
		try:
			self.identifier = int(identifier)
		except:
			raise Exception("In message: {}, from: {}, ID not number: {}".format(self.description.name, sent_by, identifier))
		self.length = int(length)
		self.timeout = timeout if (timeout!=None) else 0
		self.period = period if (period!=None) else 0
		self.fields = []
		self.timestamp = 0
		self.lastTSdiff = 0
		self.owner = owner
		self.busname = busname
	
	def add_field(self, field):
		if (not isinstance(field, Candb_msg_field)):
			raise Exception("field be instance of class candb_msg_field")
		
		if (len(self.fields) > 0 and self.fields[-1].isMux()):
			#previous field is MUX, so this field us "subfield" of it
			self.fields[-1].addMuxedSubfield(field)
		else:
			self.fields.append(field)

	def parseFromPacket(self, dataList, timestamp):
		self.lastTSdiff = timestamp - self.timestamp
		self.timestamp = timestamp
		for f in self.fields:
			f.parseFromPacket(dataList)
	
	def assemble(self, values, buff):
		# print("assembling message {}".format(self.description.name))
		if not isinstance(values, list):
			raise Exception("values must be list type, was {}".format(type(values)))
		
		if len(self.fields) != len(values):
			raise Exception("length of values ({}) is different than count of fields ({})".format(len(self.fields), len(values)))

		i = 0
		while len(values) > 0:
			values = self.fields[i].assemble(values, buff)
			i += 1
		if i != len(self.fields):
			raise Exception("something went wrong, value list not empty!")
		# print("assembly done! data:", end="")
		# for b in buff:
		# 	print("{:2X} ".format(b), end="")
		# print("")
	
	def getTimestamp(self):
		return self.timestamp
	
	def getTSdiff(self):
		return self.lastTSdiff
	
	def __str__(self):
		buff = ""
		if len(self.fields) == 0:
			buff = "\tNo FILEDS :("
		else:
			for f in self.fields:
				buff += "{}\n".format(f)

		if len(self.sent_by) > 1:
			sender = self.sent_by
		else:
			sender = self.sent_by
		return "{:4} {} {}\n{}".format(self.identifier, sender, self.description.name, buff)


class CanDB():
	def __init__(self, jsonList, debug_parsing = False):
		#follows parsing of json
		self.parsed_messages={}
		self.parsed_enums=[]

		for jFileName in jsonList: #iterate over all json files
			with open(jFileName, 'r') as jfile:
				data=jfile.read()

			# parse file
			j = json.loads(data)
			if (j.get("version") != 2):
				raise Exception("Wrong json version, expected {:d}, got {:d}".format(2, j.get("version")))
			for pkg in j.get("packages"):
				#for now, we don't distinguish between packages
				for unit in pkg.get("units"): #iterate over units (owners of messages)	
					msg_owner = unit.get("name")

					for j_enum in unit.get("enum_types"): #parse enums
						enum=Custom_enum("{}_{}".format(msg_owner, j_enum.get("name")))
						for item in j_enum.get("items"):
							enum.append(Custom_enum_element( "{}".format(item.get("name")), item.get("value"), item.get("description")))
						self.parsed_enums.append(enum)

					for msg in unit.get("messages"):
						# print(msg)
						# try:
						bus = msg.get("bus")
						if bus == None:
							bus = "UNDEFINED"

						sent_by_full = sent_by=msg.get("sent_by")
						sent_by = [src.split(".")[-1] for src in sent_by_full]#ignore parent package

						m = Candb_msg(	description=Description(msg.get("name"), text=msg.get("description")),
										owner = msg_owner,
										sent_by=sent_by,#ignore parent package 
										identifier=msg.get("id"),
										length=msg.get("length"),
										timeout=msg.get("timeout"),
										period=msg.get("tx_period"),
										busname=bus.split(".")[-1])#full bus name includes package name, ignore it
						print("message: ",m)
						# except Exception as e:
						# 	print("\tInvalid message, ",e)
						# 	#traceback.print_exc()
						# 	continue
						
						if(len(m.sent_by)==0):
							print("\tmessage ignored, nobody sends it...")
							continue

						for field in msg.get("fields"):
							name = field.get("name")
							# print("\tfield ",name)
							if(field.get("type") == "reserved"):
								pass

							elif(name != None and len(name)>0):
								# print("ftype",field.get("type"))
								if (len(name)>0):
									f = Candb_msg_field(	description=Description(field.get("name"), text=field.get("description")),
															field_type=field.get("type"),
															count=field.get("count"),
															bits=field.get("bits"),
															pos_offset=field.get("start_bit"),
															unit=field.get("unit"),
															vmin=field.get("min"),
															vmax=field.get("max"),
															voffset=field.get("offset"),
															vfactor=field.get("factor_num"),
															available_enums=self.parsed_enums)
									# print("adding field: ",f)
									m.add_field(f)
								else:
									if (debug_parsing):
										print("\tnoname field, ignored!")	
						print("message: ",m)
						
						self.parsed_messages[m.identifier] = m
	
	def isMsgKnown(self, msgId):
		return msgId in self.parsed_messages

	def getMsgById(self, msgId):
		return self.parsed_messages[msgId]
	
	def parseData(self, msgId, dataList, timestamp):
		if not self.isMsgKnown(msgId):
			return#ignore unknown message
		
		if not isinstance(timestamp, int) and not isinstance(timestamp, float):
			raise Exception("timestamp must be number, but it was {}".format(type(timestamp)))

		msg = self.getMsgById(msgId)
		msg.parseFromPacket(list(dataList), timestamp)
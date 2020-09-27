#!/usr/bin/env python3
"""
Main (reference) ocarina API, the only one officially supported!
"""

API_VERSION = 1

import serial, struct
from time import sleep

class CMD:
	AUTO_BITRATE =          b'a'
	SET_BITRATE =           b'b'
	QUERY_COUNTERS =        b'c'
	RESET_COUNTERS =        b'C'
	DFU =                   b'd'#//reboot to DFU BL
	QUERY_ERROR_FLAGS =     b'e'
	RX_FORWARDING_ENABLE =  b'F'
	RX_FORWARDING_DISABLE = b'f'
	QUERY_CONFIG =          b'g'
	QUERY_INTERFACE_ID =    b'i'
	SEND_MESSAGE_STD_ID =   b'm'
	SEND_MESSAGE_EXT_ID =   b'M'
	NOP =                   b'n'
	LOOPBACK_ENABLE =       b'L'
	LOOPBACK_DISABLE =      b'l'
	RESET =                 b'r'#//writes 24x 0xAA, host expects, there can't be same regular response
	SILENT_ENABLE =         b'S'
	SILENT_DISABLE =        b's'
	QUERY_VERSION =         b'v'
	
class DTH:
	INTERFACE_ID =      CMD.QUERY_INTERFACE_ID
	ERROR_FLAGS =       CMD.QUERY_ERROR_FLAGS
	ERROR_ON_CAN =      b'E'
	MESSAGE_STD_ID =    CMD.SEND_MESSAGE_STD_ID
	MESSAGE_EXT_ID =    CMD.SEND_MESSAGE_EXT_ID
	CONFIG =            CMD.QUERY_CONFIG
	VERSION =           CMD.QUERY_VERSION
	COUNTERS =          CMD.QUERY_COUNTERS
	HEARTBEAT =         b'h'

class PROTOCOL:
	VERSION = 3

class BITRATE:
	UNKNOWN =   0
	MIN =       1
	KBIT10 =    1
	KBIT20 =    2
	KBIT50 =    3
	KBIT100 =   4
	KBIT125 =   5
	KBIT250 =   6
	KBIT500 =   7
	KBIT800 =   8
	KBIT1000 =  9
	MAX   =     9
	KBITnumeric = [None, 10, 20, 50, 100, 125, 250, 500, 800, 1000]

class ERRORFLAG:
	USBIN_OVERFLOW          = 0
	USBOUT_OVERFLOW         = 1
	CANIN_OVERFLOW          = 2
	CANOUT_OVERFLOW         = 3
	INVALID_CAN_MSG_REQUEST = 4
	INVALID_COMMAND         = 5
	NOT_INITIALIZED_TX      = 6
	INVALID_TLV             = 7
	TOO_LONG_TLV            = 8

class CANERROR:
	#0~7 direct mapping of LEC
	NONE =            0
	STUFF =           1
	FORM =            2
	ACKNOWLEDGMENT =  3
	BIT_RECESSIVE =   4
	BIT_DOMINANT =    5
	CRC =             6
	SET_BY_SW =       7

	UNKNOWN =        15

	def __init__(self, value):
		self.value = value
	
	def __str__(self):
		if self.value == self.NONE:
			return "NONE"
		if self.value == self.STUFF:
			return "STUFF"
		if self.value == self.FORM:
			return "FORMATTING"
		if self.value == self.ACKNOWLEDGMENT:
			return "ACKNOWLEDGMENT"
		if self.value == self.BIT_RECESSIVE:
			return "BIT RECESSIVE"
		if self.value == self.BIT_DOMINANT:
			return "BIT DOMINANT"
		if self.value == self.CRC:
			return "CRC"
		if self.value == self.SET_BY_SW:
			return "SET BY SW"
		if self.value == self.UNKNOWN:
			return "UNKNOWN"
		else:
			return "NOT SUPPORTED"
		

class CANBUSSTATE:
	OK =      0
	PASSIVE = 1
	OFF =     2

	def __init__(self, value):
		self.value = value
	
	def __str__(self):
		if self.value == self.OK:
			return "OK"
		if self.value == self.PASSIVE:
			return "PASSIVE"
		if self.value == self.OFF:
			return "OFF"
		else:
			return "NOT SUPPORTED"
		


SYNC_FRAME = [\
	0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,\
	0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,\
	0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA]

class IDTYPE:
	STD = 0
	EXT = 1

#data classes
class CanMsgID:
	def __init__(self, value, idType):
		self.value = value
		if(idType not in [IDTYPE.STD, IDTYPE.EXT]):
			raise TypeError("Bad ID type")
		self.type = idType


class Bitrate:
	"""
	Holds bitrate information
	
	:param enum: enum value of bitrate, defaults to None
	:type enum: integer, optional
	:param kbits: bitrate in kbits, defaults to None
	:type kbits: integer, optional
	:raises ValueError: invalid enum value
	:raises ValueError: invalid kbits value
	:raises Exception: neither enum nor kbits given
	"""
	enum = None		#: holds enum value of bitrate
	kbits = None	#: holds bitrate value in kbits
	def __init__(self, enum = None, kbits = None):
		if enum != None:
			self.enum = enum
			if enum == BITRATE.UNKNOWN:
				self.kbits = None
			elif enum < BITRATE.MIN or enum > BITRATE.MAX:
				raise ValueError("wrong bitrate enum")
			else:
				self.kbits = BITRATE.KBITnumeric[enum]
		elif kbits != None:
			if(kbits not in BITRATE.KBITnumeric):
				raise ValueError("bitrate {:d} is not supported".format(kbits))
			self.kbits = kbits
			self.enum = BITRATE.KBITnumeric.index(kbits)
		else:
			raise Exception("define at least one of enum or kbits")
	


#event classes
class Event:
	pass

class HeartbeatEvent(Event):
	def __str__(self):
		return "Heartbeat Event"

class VersionEvent(Event):
	def __init__(self, protocol, sw, hw, hwRevision):
		self.protocol = protocol
		self.sw = sw
		self.hw = hw
		self.hwRevision = hwRevision
	
	def __str__(self):
		return 'VersionEvent(Protocol {:2d} | SW {:2d} | HW {:2d} | HWrev {:2d}'.format(self.protocol, self.sw, self.hw, self.hwRevision)


class CanMsgEvent(Event):
	def __init__(self, id, data, timestamp):
		self.id = id
		self.data = data
		self.timestamp = timestamp

	def __str__(self):
		return 'CanMsgEvent({} @ {:13.6f} ID: {:8X} data({:2d}): {})'.format("STD" if self.id.type == IDTYPE.STD else "EXT", self.timestamp/1e6, self.id.value, len(self.data), ' '.join(['{:02X}'.format(b) for b in self.data]))

class CANErrorEvent(Event):
	def __init__(self, TEC, REC, busState, eType, timestamp):
		self.TEC = TEC
		self.REC = REC
		self.busState = busState
		self.eType = eType
		self.timestamp = timestamp

	def __str__(self):
		return 'CanErrorEvent(ERR @ {:4.6f} error: {} busState: {}, TEC: {:3d}, REC: {:3d})'.format(self.timestamp/1e6, self.eType, self.busState, self.TEC, self.REC)

class ErrorFlagsEvent(Event):
	def __init__(self, value):
		self.value = value

	def __str__(self):
		return 'ErrorFlagsEvent(0x{:04X})'.format(self.value)

class IfaceIdEvent(Event):
	def __init__(self, if_id):
		self.if_id = if_id

	def __str__(self):
		return 'IfaceIdEvent({:s})'.format(self.if_id)

class ConfigEvent(Event):
	def __init__(self, bitrate, silent, loopback, forward):
		self.bitrate = bitrate
		self.silent = silent
		self.loopback = loopback
		self.forward = forward

	def __str__(self):
		return 'ConfigEvent( enum {:d} = {} kbit/s, silent({}) | loopback({}) | forward({}))'.format(
			self.bitrate.enum, self.bitrate.kbits,
			"X" if self.silent else " ",
			"X" if self.loopback else " ",
			"X" if self.forward else " ")

class ProtocolEvent(Event):
	def __init__(self, protocol):
		self.version = int(protocol)

	def __str__(self):
		return 'ProtocolEvent({:d})'.format(self.version)

class CountersEvent(Event):
	def __init__(self, rxed, txed):
		self.rxed = rxed
		self.txed = txed

	def __str__(self):
		return 'CountersEvent(RX: {:6d}, TX: {:6d})'.format(self.rxed, self.txed)

class Payload:
	def __init__(self, data):
		if(not isinstance(data, bytes)):
			raise TypeError("payload must be in bytes")
		self.data = data
	
	def pop(self, amount = None):
		if amount == None:
			requested = self.data
			self.data = []
		else:
			requested, self.data = self.data[0:amount], self.data[amount:]
		return requested
	
	def popu8(self):
		return struct.unpack('B', self.pop(1))[0]
	
	def popu16(self):
		return struct.unpack('H', self.pop(2))[0]
	
	def popu32(self):
		return struct.unpack('I', self.pop(4))[0]
	
	def isEmpty(self):
		return len(self.data) == 0
	
	def length(self):
		return len(self.data)
	
	def __str__(self):
		return str(self.data)

class Ocarina:
	def __init__(self, port):
		"""
		Ocarina API class

		:param port: identifier of com port (/dev/ttyACM0, /dev/ocarina, COM1, ....)
		:type port: string
		"""
		self.ser = None
		self.ser = serial.Serial(port, 115200, timeout=3)
		print("device {} opened".format(port))

		# flush RX buffers
		self.nop()#hack to trigger potential input buffer autoflush when there is some garbage
		self._connect()

	def __del__(self):
		if(self.ser != None):
			self.set_message_forwarding(False)
			self.ser.close()

	def _close(self):
		self.ser.close()
	
	def _write(self, data):
		return self.ser.write(data)
	
	def _read(self, length):
		return self.ser.read(length)
	
	def _write_command(self, command, data = bytes()):
		return self._write(command+bytes([len(data)])+data)



	def read_event(self, blocking = False, forwardHeartbeat = True):
		"""
		Must be called periodically by user. Handles incoming events from device.
		
		:param blocking: should return on valid event only?, defaults to False
		:type blocking: bool, optional
		:return: received event
		:rtype: instance of any Event class or None if timeouted in non blocking mode
		"""
		while True:
			frameType = self._read(1)
			if(len(frameType)==0):
				print("timeouted")
				continue

			frameLength = struct.unpack('B', self._read(1))[0]
			# print("type: {}, length: {}".format(frameType, frameLength))
			payload = Payload(self._read(frameLength))

			if frameType == DTH.INTERFACE_ID:
				return IfaceIdEvent(payload.data.decode())

			elif frameType == DTH.MESSAGE_STD_ID:
				ts = payload.popu8() + (payload.popu32()<<8)#in us
				sid = CanMsgID(payload.popu16(), IDTYPE.STD)
				data = payload.pop()
				return CanMsgEvent(sid, data, ts)

			elif frameType == DTH.MESSAGE_EXT_ID:
				ts = payload.popu8() + (payload.popu32()<<8)#in us
				eid = CanMsgID(payload.popu32(), IDTYPE.EXT)
				data = payload.pop()
				return CanMsgEvent(eid, data, ts)
			
			elif frameType == DTH.ERROR_ON_CAN:
				ts = payload.popu8() + (payload.popu32()<<8)#in us
				TEC = payload.popu8()
				REC = payload.popu8()
				mixed = payload.popu8()
				state = CANBUSSTATE(mixed&0xF)
				eType = CANERROR(mixed>>4)
				return CANErrorEvent(TEC, REC, state, eType, ts)

			elif frameType == DTH.ERROR_FLAGS:
				value = payload.popu32()
				return ErrorFlagsEvent(value)
			
			elif frameType == DTH.COUNTERS:
				rxed = payload.popu32()
				txed = payload.popu32()
				return CountersEvent(rxed, txed)

			elif frameType == DTH.HEARTBEAT:
				if forwardHeartbeat:
					return HeartbeatEvent()
			
			elif frameType == DTH.CONFIG:
				raw = payload.popu8()
				return ConfigEvent( Bitrate(enum = raw & 0xF) , bool(raw & (1<<4)), bool(raw & (1<<5)), bool(raw & (1<<6)))
			
			elif frameType == DTH.VERSION:
				protocol = payload.popu8()
				sw = payload.popu8()
				hw = payload.popu8()
				hwRevision = payload.popu8()
				return VersionEvent(protocol, sw, hw, hwRevision)

			else:
				print("UNKNOWN event. {}".format(frameType))
			
			if(not payload.isEmpty()):
				print("response payload remaining")#some port of response was not read
			
			if not blocking:
				return None
	
	def _connect(self):
		"""
		Initializes device by resetting CAN iface config and synchronizing HOST-DEVICE communications
		"""
		self._write_command(CMD.RESET)
		sync_reply = []
		cnt = 0
		while(sync_reply != SYNC_FRAME):
			cnt += 1
			sync_reply.append(ord(self._read(1)))
			if len(sync_reply) > len(SYNC_FRAME):
				sync_reply = sync_reply[-len(SYNC_FRAME):]#match length to extecped SYNC_FRAME length by leaving last N elements
		print("Initialized, skipped bytes: {:d}".format(cnt-len(SYNC_FRAME)))

	def set_message_forwarding(self, enable):
		"""
		Configures, if received messages are forwarded to host
		
		:param enable: requested state of forwarding
		:type enable: bool
		"""
		if(enable):
			self._write_command(CMD.RX_FORWARDING_ENABLE)
		else:
			self._write_command(CMD.RX_FORWARDING_DISABLE)
	
	def set_loopback(self, enable):
		"""
		Configures, if transmitted messages are also received (looped back)
		
		:param enable: requested state of loopback mode
		:type enable: bool
		"""
		if(enable):
			self._write_command(CMD.LOOPBACK_ENABLE)
		else:
			self._write_command(CMD.LOOPBACK_DISABLE)
	
	def set_silent(self, enable):
		"""
		Configures, if device may touch CAN bus at all
		
		:param enable: requested state of silent mode
		:type enable: bool
		"""
		if(enable):
			self._write_command(CMD.SILENT_ENABLE)
		else:
			self._write_command(CMD.SILENT_DISABLE)


	def send_message_std(self, sid, data = []):
		"""
		Send STD ID message to CAN bus
		
		:param sid: STD ID of message
		:type sid: integer 11 bit
		:param data: message data, defaults to []
		:type data: list of integers <0;255>, optional
		:raises TypeError: if sid is not integer
		:raises ValueError: if sid >= 2^11
		:raises ValueError: if len(data) > 8
		"""
		if (not isinstance(sid, int)):
			raise TypeError("sid must have integer type")
		if (sid >= 2**11):
			raise ValueError("sid must be in range <{0:4d}[{0:3X}];{1:4d}[{1:3X}]>",format(0, 2**11-1))
		if (len(data) > 8):
			raise ValueError("data too long")
		self._write_command(CMD.SEND_MESSAGE_STD_ID, \
		struct.pack('H', sid) + bytes(data))

	def send_message_ext(self, eid, data = []):
		"""
		Send EXT ID message to CAN bus
		
		:param eid: EXT ID of message
		:type eid: integer 29 bit
		:param data: message data, defaults to []
		:type data: list of integers <0;255>, optional
		:raises TypeError: if eid is not integer
		:raises ValueError: if eid >= 2^29
		:raises ValueError: if len(data) > 8
		"""
		if (not isinstance(eid, int)):
			raise TypeError("eid must have integer type")
		if (eid >= 2**29):
			raise ValueError("eid must be in range <{0:4d}[{0:3X}];{1:4d}[{1:3X}]>",format(0, 2**29-1))
		if (len(data) > 8):
			raise ValueError("data too long")
		self._write_command(CMD.SEND_MESSAGE_EXT_ID, \
		struct.pack('I', eid) + bytes(data))

	def query_error_flags(self):
		"""
		query error flags from device and clear them
		"""
		self._write_command(CMD.QUERY_ERROR_FLAGS)
	
	def query_config(self):
		"""
		query current config from device (bitrate, silent, loopback, forwarding)
		"""
		self._write_command(CMD.QUERY_CONFIG)

	def query_interface(self):
		"""
		query device identification string
		"""
		self._write_command(CMD.QUERY_INTERFACE_ID)
	
	def query_version(self):
		"""
		query device versions (protocol, SW, HW, HW rev.)
		"""
		self._write_command(CMD.QUERY_VERSION)

	def query_counters(self):
		"""
		query device counters
		"""
		self._write_command(CMD.QUERY_COUNTERS)
	
	def reset_counters(self):
		"""
		reset device counters
		"""
		self._write_command(CMD.RESET_COUNTERS)
	
	def set_bitrate_manual(self, bitrate):
		"""
		set bitrate manually
		
		:param bitrate: requested bitrate
		:type bitrate: instance of Bitrate class
		"""
		self._write_command(CMD.SET_BITRATE, bytes([bitrate.enum]))
	
	def set_bitrate_auto(self):
		"""
		switch to auto bitrate mode
		"""
		self._write_command(CMD.AUTO_BITRATE)

	def reboot_to_DFU(self):
		"""
		reboot to bootloader, useful for flashing new version
		"""
		self._write_command(CMD.DFU)
	
	def nop(self):
		"""
		no operation command, useful to trigger ocarina input buffer autoflush when there is some garbage
		"""
		self._write_command(CMD.NOP)
	
	@staticmethod
	def getAPIversion():
		return API_VERSION

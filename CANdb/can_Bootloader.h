#ifndef CAN_BOOTLOADER_H
#define CAN_BOOTLOADER_H

#include <tx2/can.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    bus_CAN1 = 0,
    bus_CAN2 = 1,
    bus_UNDEFINED = 2,
};

enum { Bootloader_EntryReq_id = 0x5F0 };
enum { Bootloader_EntryAck_id = 0x5F1 };
enum { Bootloader_BootloaderBeacon_id = 0x5F2 };
enum { Bootloader_Data_id = 0x5F3 };
enum { Bootloader_ExitReq_id = 0x5F5 };
enum { Bootloader_DataAck_id = 0x5F8 };
enum { Bootloader_Handshake_id = 0x5FA };
enum { Bootloader_HandshakeAck_id = 0x5FB };
enum { Bootloader_SoftwareBuild_id = 0x5FD };
enum { Bootloader_SerialOutput_id = 0x5FF };

enum Bootloader_BootTarget {
    /* Accumulator management System */
    Bootloader_BootTarget_AMS = 0,
};

enum Bootloader_HandshakeResponse {
    /* Everything is ok */
    Bootloader_HandshakeResponse_OK = 0,
    /* Given address is not aligned to page boundary */
    Bootloader_HandshakeResponse_PageAddressNotAligned = 1,
    /* Given address is not located in flash at all */
    Bootloader_HandshakeResponse_AddressNotInFlash = 2,
    /* Given page belongs to the bootloader. */
    Bootloader_HandshakeResponse_PageProtected = 3,
    /* The specified number of pages to erase (sent during handshake) and the actual number of erase requests do not match. Reported only after the erasing process ends. */
    Bootloader_HandshakeResponse_ErasedPageCountMismatch = 4,
};

enum Bootloader_Register {
    /* Address of the entry point of flashed firmware. */
    Bootloader_Register_EntryPoint = 0,
    /* Address of the isr vector. Must be aligned to 512B boundary (lower 9 bits cleared) */
    Bootloader_Register_InterruptVector = 1,
    /* The size of available flash memory in bytes. */
    Bootloader_Register_FlashSize = 2,
    /* The number of flash pages to be erased */
    Bootloader_Register_NumPagesToErase = 3,
    /* Address of a flash page to erase */
    Bootloader_Register_PageToErase = 4,
};

enum Bootloader_State {
    /* Bootloader is ready to commence handshake with the flashing system */
    Bootloader_State_Ready = 0,
    /* The device and the flashing master are performing handshake sequence */
    Bootloader_State_Handshake = 1,
    /* Bootloader is currently receiving bytes over the bus */
    Bootloader_State_ReceivingData = 2,
    /* Device is busy flashing received data */
    Bootloader_State_Flashing = 3,
    /* Some error occured. //TODO make it more concrete */
    Bootloader_State_Error = 4,
};

enum Bootloader_WriteStatus {
    /* Write was performed successfully. */
    Bootloader_WriteStatus_Ok = 0,
    /* Requested address was not in any of the beforehand specified erasable pages */
    Bootloader_WriteStatus_NotInErasableMemory = 1,
    /* Requested address was not in the flash */
    Bootloader_WriteStatus_NotInFlash = 2,
    /* Requested address has already been written */
    Bootloader_WriteStatus_NotWriteable = 3,
};

/*
 * Request for an ECU to enter the bootloader.
 */
typedef struct Bootloader_EntryReq_t {
	/* Identifies the unit targeted by this message. */
	enum Bootloader_BootTarget	Target;

	/* Sequence counter, incremented each time a message is sent. */
	uint8_t	SEQ;
} Bootloader_EntryReq_t;

#define Bootloader_EntryReq_SEQ_OFFSET	((float)0)
#define Bootloader_EntryReq_SEQ_FACTOR	((float)1)
#define Bootloader_EntryReq_SEQ_MIN	((float)0)
#define Bootloader_EntryReq_SEQ_MAX	((float)15)

/*
 * Targeted unit has received a bootloader entry request and has reset pending.
 */
typedef struct Bootloader_EntryAck_t {
	/* Identifies the unit which is currently transitioning to the bootloader. */
	enum Bootloader_BootTarget	Target;
} Bootloader_EntryAck_t;


/*
 * Sent periodically by the unit with active bootloader to announce its presence.
 */
typedef struct Bootloader_BootloaderBeacon_t {
	/* Identifies which unit has active bootloader */
	enum Bootloader_BootTarget	Unit;

	/* Current state of the bootloader */
	enum Bootloader_State	State;
} Bootloader_BootloaderBeacon_t;


/*
 * Stream of data to be flashed into the MCU.
 */
typedef struct Bootloader_Data_t {
	/* Word (4B) aligned address where the data shall be written */
	uint32_t	Address;

	/* If set to true, only the lower 16 bits of Word will be flashed. */
	uint8_t	HalfwordAccess;

	/* Word to be written */
	uint32_t	Word;
} Bootloader_Data_t;

#define Bootloader_Data_Address_OFFSET	((float)0)
#define Bootloader_Data_Address_FACTOR	((float)1)
#define Bootloader_Data_Address_MIN	((float)0)

/*
 * Request from the flash master to an ECU to exit the bootloader.
 */
typedef struct Bootloader_ExitReq_t {
	/* Which unit shall leave the bootloader. */
	enum Bootloader_BootTarget	Target;
} Bootloader_ExitReq_t;


/*
 * Acknowledgement that the bootloader successfully received word of data. It is not necessary that the word has already been written.
 */
typedef struct Bootloader_DataAck_t {
	/* Word (4B) aligned address of last write comand. */
	uint32_t	Address;

	/* Identifies the result of previous write operation. */
	enum Bootloader_WriteStatus	Result;
} Bootloader_DataAck_t;


/*
 * Configuration message sent by the flashing master to the bootloader.
 */
typedef struct Bootloader_Handshake_t {
	/* Which value (lets call them registers) is currently configured */
	enum Bootloader_Register	Register;

	/* Value for selected register */
	uint32_t	Value;
} Bootloader_Handshake_t;


/*
 * Acknowledgement that a Handshake message has been received.
 */
typedef struct Bootloader_HandshakeAck_t {
	/* Last written register */
	enum Bootloader_Register	Register;

	/* Error code indicating the result of last operation */
	enum Bootloader_HandshakeResponse	Response;

	/* Last written value */
	uint32_t	Value;
} Bootloader_HandshakeAck_t;


/*
 * Information about the currently running software (SHA of git commit).
 */
typedef struct Bootloader_SoftwareBuild_t {
	/* First 8 hex digits of current git commit */
	uint32_t	CommitSHA;

	/* True iff the currently flashed code contains uncommited changes */
	uint8_t	DirtyRepo;
} Bootloader_SoftwareBuild_t;


/*
 * Packetized serial output
 */
typedef struct Bootloader_SerialOutput_t {
	/* Sequence counter */
	uint8_t	SEQ;

	/* Indicates internal buffer overflow */
	uint8_t	Truncated;

	/* Transmission complete (for now)? */
	uint8_t	Completed;

	/* Channel (0 or 1) */
	uint8_t	Channel;

	/* True iff an error occured while reading. */
	uint8_t	CouldNotRead;

	/* Unused bytes are marked with 0 */
	uint8_t	Payload[7];
} Bootloader_SerialOutput_t;

#define Bootloader_SerialOutput_SEQ_OFFSET	((float)0)
#define Bootloader_SerialOutput_SEQ_FACTOR	((float)1)
#define Bootloader_SerialOutput_SEQ_MIN	((float)0)
#define Bootloader_SerialOutput_SEQ_MAX	((float)15)
#define Bootloader_SerialOutput_Channel_MIN	((float)0)
#define Bootloader_SerialOutput_Channel_MAX	((float)1)

void candbInit(void);

int Bootloader_decode_EntryReq_s(const uint8_t* bytes, size_t length, Bootloader_EntryReq_t* data_out);
int Bootloader_decode_EntryReq(const uint8_t* bytes, size_t length, enum Bootloader_BootTarget* Target_out, uint8_t* SEQ_out);
int Bootloader_get_EntryReq(Bootloader_EntryReq_t* data_out);
void Bootloader_EntryReq_on_receive(int (*callback)(Bootloader_EntryReq_t* data));

int Bootloader_send_EntryAck_s(const Bootloader_EntryAck_t* data);
int Bootloader_send_EntryAck(enum Bootloader_BootTarget Target);

int Bootloader_send_BootloaderBeacon_s(const Bootloader_BootloaderBeacon_t* data);
int Bootloader_send_BootloaderBeacon(enum Bootloader_BootTarget Unit, enum Bootloader_State State);
int Bootloader_BootloaderBeacon_need_to_send(void);

int Bootloader_decode_Data_s(const uint8_t* bytes, size_t length, Bootloader_Data_t* data_out);
int Bootloader_decode_Data(const uint8_t* bytes, size_t length, uint32_t* Address_out, uint8_t* HalfwordAccess_out, uint32_t* Word_out);
int Bootloader_get_Data(Bootloader_Data_t* data_out);
void Bootloader_Data_on_receive(int (*callback)(Bootloader_Data_t* data));

int Bootloader_send_ExitReq_s(const Bootloader_ExitReq_t* data);
int Bootloader_send_ExitReq(enum Bootloader_BootTarget Target);

int Bootloader_send_DataAck_s(const Bootloader_DataAck_t* data);
int Bootloader_send_DataAck(uint32_t Address, enum Bootloader_WriteStatus Result);

int Bootloader_decode_Handshake_s(const uint8_t* bytes, size_t length, Bootloader_Handshake_t* data_out);
int Bootloader_decode_Handshake(const uint8_t* bytes, size_t length, enum Bootloader_Register* Register_out, uint32_t* Value_out);
int Bootloader_get_Handshake(Bootloader_Handshake_t* data_out);
void Bootloader_Handshake_on_receive(int (*callback)(Bootloader_Handshake_t* data));

int Bootloader_send_HandshakeAck_s(const Bootloader_HandshakeAck_t* data);
int Bootloader_send_HandshakeAck(enum Bootloader_Register Register, enum Bootloader_HandshakeResponse Response, uint32_t Value);

int Bootloader_send_SoftwareBuild_s(const Bootloader_SoftwareBuild_t* data);
int Bootloader_send_SoftwareBuild(uint32_t CommitSHA, uint8_t DirtyRepo);
int Bootloader_SoftwareBuild_need_to_send(void);

int Bootloader_send_SerialOutput_s(const Bootloader_SerialOutput_t* data);
int Bootloader_send_SerialOutput(uint8_t SEQ, uint8_t Truncated, uint8_t Completed, uint8_t Channel, uint8_t CouldNotRead, uint8_t* Payload);
int Bootloader_SerialOutput_need_to_send(void);

#ifdef __cplusplus
}

template <typename T>
bool need_to_send();

inline int send(const Bootloader_EntryAck_t& data) {
    return Bootloader_send_EntryAck_s(&data);
}

template <>
inline bool need_to_send<Bootloader_BootloaderBeacon_t>() {
    return Bootloader_BootloaderBeacon_need_to_send();
}

inline int send(const Bootloader_BootloaderBeacon_t& data) {
    return Bootloader_send_BootloaderBeacon_s(&data);
}

inline int send(const Bootloader_ExitReq_t& data) {
    return Bootloader_send_ExitReq_s(&data);
}

inline int send(const Bootloader_DataAck_t& data) {
    return Bootloader_send_DataAck_s(&data);
}

inline int send(const Bootloader_HandshakeAck_t& data) {
    return Bootloader_send_HandshakeAck_s(&data);
}

template <>
inline bool need_to_send<Bootloader_SoftwareBuild_t>() {
    return Bootloader_SoftwareBuild_need_to_send();
}

inline int send(const Bootloader_SoftwareBuild_t& data) {
    return Bootloader_send_SoftwareBuild_s(&data);
}

template <>
inline bool need_to_send<Bootloader_SerialOutput_t>() {
    return Bootloader_SerialOutput_need_to_send();
}

inline int send(const Bootloader_SerialOutput_t& data) {
    return Bootloader_send_SerialOutput_s(&data);
}

#endif

#endif

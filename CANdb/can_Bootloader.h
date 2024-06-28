#ifndef CAN_BOOTLOADER_H
#define CAN_BOOTLOADER_H

#include <tx2/can.h>

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif

//CANdb code model v2 (enhanced again) generated for Bootloader on 28. 06. 2024 (dd. mm. yyyy) at 02.21.47 (hh.mm.ss)

typedef enum {
    // Vehicle CAN buses
    bus_CAN1 = 0,
    bus_CAN2 = 1,
    bus_UNDEFINED = 12,
    // Other constants useful to the software
    bus_ALL = 13, // Send to all vehicle buses (e.g. unit SoftwareBuild)
    bus_FORWARD = 14, // Represents a local CAN the ECU forwards messages to (e.g. CTU24 PDL, VDCU and QUAD LV Conn).
} candb_bus_t;

enum { Bootloader_Handshake_id          = STD_ID(0x620) };
enum { Bootloader_Handshake_tx_bus      = bus_UNDEFINED };
enum { Bootloader_HandshakeAck_id       = STD_ID(0x621) };
enum { Bootloader_HandshakeAck_tx_bus   = bus_UNDEFINED };
enum { Bootloader_CommunicationYield_id = STD_ID(0x622) };
enum { Bootloader_CommunicationYield_tx_bus = bus_UNDEFINED };
enum { Bootloader_Data_id               = STD_ID(0x623) };
enum { Bootloader_Data_tx_bus           = bus_UNDEFINED };
enum { Bootloader_DataAck_id            = STD_ID(0x624) };
enum { Bootloader_DataAck_tx_bus        = bus_UNDEFINED };
enum { Bootloader_ExitReq_id            = STD_ID(0x625) };
enum { Bootloader_Ping_id               = STD_ID(0x626) };
enum { Bootloader_Beacon_id             = STD_ID(0x627) };
enum { Bootloader_Beacon_timeout        = 200 };
enum { Bootloader_Beacon_period         = 50 };
enum { Bootloader_Beacon_tx_bus         = bus_ALL };
enum { Bootloader_PingResponse_id       = STD_ID(0x629) };
enum { Bootloader_PingResponse_tx_bus   = bus_UNDEFINED };
enum { Bootloader_ExitAck_id            = STD_ID(0x62A) };
enum { Bootloader_ExitAck_tx_bus        = bus_UNDEFINED };
enum { Bootloader_SoftwareBuild_id      = STD_ID(0x62D) };
enum { Bootloader_SoftwareBuild_period  = 1000 };
enum { Bootloader_SoftwareBuild_tx_bus  = bus_ALL };

extern CAN_ID_t const candb_sent_messages[9];
extern CAN_ID_t const candb_received_messages[8];

enum Bootloader_BootTarget {
    /* Accumulator management System */
    Bootloader_BootTarget_AMS = 0,
    /* Dashboard */
    Bootloader_BootTarget_DSH = 1,
    /* QUAD MC enclosure connector PCB */
    Bootloader_BootTarget_QUADCONN = 2,
    /* Pedal Box */
    Bootloader_BootTarget_PDL = 3,
    /* Fuse Box */
    Bootloader_BootTarget_FSB = 4,
    /* Steering wheel */
    Bootloader_BootTarget_STW = 5,
    /* Emergency Brake System Supervisor (DV only) */
    Bootloader_BootTarget_EBSS = 6,
    /* SiC motor controller front */
    Bootloader_BootTarget_DRTF = 7,
    /* Steering Actuator (DV only) */
    Bootloader_BootTarget_SA = 8,
    /* SiC motor controller rear */
    Bootloader_BootTarget_DRTR = 9,
    /* Left wing Measurebox */
    Bootloader_BootTarget_MBOXL = 10,
    /* Right wing Measurebox */
    Bootloader_BootTarget_MBOXR = 11,
    /* Danil's beloved telemetry */
    Bootloader_BootTarget_TLM = 12,
    /* Vehicle Dynamics Control Unit */
    Bootloader_BootTarget_VDCU = 13,
    /* Anti-roll bar control unit */
    Bootloader_BootTarget_ARB = 14,
    /* Brake balance control */
    Bootloader_BootTarget_BB = 15,
};

enum Bootloader_Command {
    /* Nothing needs to be done right now */
    Bootloader_Command_None = 0,
    /* Sent by master to initiate FLASHING transaction */
    Bootloader_Command_StartTransactionFlashing = 1,
    /* Something failed horribly. Kill the process */
    Bootloader_Command_AbortTransaction = 2,
    /* Temporarily stop the transmission (e.g. the target device is overrun) */
    Bootloader_Command_StallSubtransaction = 3,
    /* Resume previously paused subtransaction */
    Bootloader_Command_ResumeSubtransaction = 4,
    /* Some data packets have been lost. Return to specified address and start again */
    Bootloader_Command_RestartFromAddress = 5,
    /* The bootloader itself shall be erased and flashed. */
    Bootloader_Command_StartBootloaderUpdate = 6,
    /* Update the application jump table with new value of the isr vector. If the jump table was not valid, fill other metadata with zeros. */
    Bootloader_Command_SetNewVectorTable = 7,
};

enum Bootloader_EntryReason {
    /* The bootloader is listening for pings over CAN messages to determine, whether it is allowed to start the application */
    Bootloader_EntryReason_StartupCanBusCheck = 0,
    /* The bootloader will not be initialized, application is started right away */
    Bootloader_EntryReason_DontEnter = 1,
    /* The application's interrupt vector is not aligned properly */
    Bootloader_EntryReason_InterruptVectorNotAligned = 2,
    /* The entry point pointer does not point into flash */
    Bootloader_EntryReason_EntryPointNotInFlash = 3,
    /* The vector table pointer not point into flash */
    Bootloader_EntryReason_InterruptVectorNotInFlash = 4,
    /* The specified top of stack does not point to RAM */
    Bootloader_EntryReason_TopOfStackInvalid = 5,
    /* The value contained in the boot control register was not recognized */
    Bootloader_EntryReason_BackupRegisterCorrupted = 6,
    /* The bootloader was requested by the flash master via message Ping either during normal application operation or during startup CAN bus check */
    Bootloader_EntryReason_Requested = 7,
    /* The application repeatedly crashed and cannot continue (it was unable to properly initialize or encountered fatal error like a failed assertion) */
    Bootloader_EntryReason_ApplicationFailure = 8,
    /* The entry reason is not known */
    Bootloader_EntryReason_Unknown = 9,
    /* No firmware is flashed, the jump table is erased */
    Bootloader_EntryReason_ApplicationMissing = 10,
    /* The jump table contains some data but is not valid */
    Bootloader_EntryReason_JumpTableCorrupted = 11,
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
    /* Available flash memory cannot fit the requested number of bytes. */
    Bootloader_HandshakeResponse_BinaryTooBig = 5,
    /* Received address of the interrupt vector is not aligned */
    Bootloader_HandshakeResponse_InterruptVectorNotAligned = 6,
    /* Written transaction magic did not match the expected value. */
    Bootloader_HandshakeResponse_InvalidTransactionMagic = 7,
    /* Handshake is performed out of order. */
    Bootloader_HandshakeResponse_HandshakeSequenceError = 8,
    /* The specified page was already erased */
    Bootloader_HandshakeResponse_PageAlreadyErased = 9,
    /* There are not that many pages available */
    Bootloader_HandshakeResponse_NotEnoughPages = 10,
    /* The number of bytes written does not match the number of expected bytes */
    Bootloader_HandshakeResponse_NumWrittenBytesMismatch = 11,
    /* The specified entry point does not match the second word in interrupt vector. */
    Bootloader_HandshakeResponse_EntryPointAddressMismatch = 12,
    /* Received checksum of the firmware does not match the computed checksum. */
    Bootloader_HandshakeResponse_ChecksumMismatch = 13,
    /* The specified number of logical blocks does not fit into the internal bootloader's memory */
    Bootloader_HandshakeResponse_TooManyLogicalMemoryBlocks = 14,
    /* The bootloader expected transaction type selection, but the master sent something else */
    Bootloader_HandshakeResponse_UnknownTransactionType = 15,
    /* The bootloader received some handshake message when it was the transmitting bus master */
    Bootloader_HandshakeResponse_HandshakeNotExpected = 16,
    /* The bootloader detected inconsistent internal state */
    Bootloader_HandshakeResponse_InternalStateMachineError = 17,
    /* Register is not command but command is not none */
    Bootloader_HandshakeResponse_CommandNotNone = 18,
    /* Bootloader or some module is in error state. */
    Bootloader_HandshakeResponse_BootloaderInError = 19,
    /* This command is not valid right now. */
    Bootloader_HandshakeResponse_CommandInvalidInCurrentContext = 20,
    /* Number of expected and received logical memory blocks do not match */
    Bootloader_HandshakeResponse_LogicalBlockCountMismatch = 21,
    /* Two received logical memory blocks are overlapping */
    Bootloader_HandshakeResponse_LogicalBlocksOverlapping = 22,
    /* The start address of received logical memory block < end address of previous memory block */
    Bootloader_HandshakeResponse_LogicalBlockAddressesNotIncreasing = 23,
    /* Received logical block cannot be covered using available physical blocks */
    Bootloader_HandshakeResponse_LogicalBlockNotCoverable = 24,
    /* The specified address is not located within the address space reserved for the bootloader. */
    Bootloader_HandshakeResponse_AddressNotInBootloader = 25,
    /* Received logical memory block's length exceeds remaining flash memory. */
    Bootloader_HandshakeResponse_LogicalBlockTooLong = 26,
    /* Specified register does not accept value zero (number of bytes, number of pages or something similar). */
    Bootloader_HandshakeResponse_MustBeNonZero = 27,
};

enum Bootloader_Register {
    /* Address of the entry point of flashed firmware. */
    Bootloader_Register_EntryPoint = 0,
    /* Address of the isr vector. Must be aligned to 512B boundary (lower 9 bits cleared) */
    Bootloader_Register_InterruptVector = 1,
    /* The number of flash physical memory blocks to be erased */
    Bootloader_Register_NumPhysicalBlocksToErase = 2,
    /* Address of a flash physical memory blockto erase */
    Bootloader_Register_PhysicalBlockToErase = 3,
    /* Size in bytes of the firmware to be flashed. */
    Bootloader_Register_FirmwareSize = 4,
    /* Checksum of the firmware. Taken as the sum of all transmitted halfwords (16bit) */
    Bootloader_Register_Checksum = 5,
    /* Magic value. Writing 0x696c6548 starts and ends the transaction. */
    Bootloader_Register_TransactionMagic = 6,
    /* Number of memory blocks constituting the memory map of firmware */
    Bootloader_Register_NumLogicalMemoryBlocks = 7,
    /* The starting address for given logical memory block */
    Bootloader_Register_LogicalBlockStart = 8,
    /* The length in bytes of given logical memory block */
    Bootloader_Register_LogicalBlockLength = 9,
    /* Number of physical memory blocks constituting the available flash memory */
    Bootloader_Register_NumPhysicalMemoryBlocks = 10,
    /* The starting address for given physical memory block */
    Bootloader_Register_PhysicalBlockStart = 11,
    /* The length in bytes of given physical memory block */
    Bootloader_Register_PhysicalBlockLength = 12,
    /* Use the Command field in message Handshake to determine the requested task */
    Bootloader_Register_Command = 13,
};

enum Bootloader_State {
    /* Bootloader is ready to commence handshake with the flashing system */
    Bootloader_State_Ready = 0,
    /* Master sent initial transaction magic and the bootloader is now awaiting the selection of transaction type */
    Bootloader_State_Initialization = 1,
    /* BL is sending the available memory map */
    Bootloader_State_TransmittingPhysicalMemoryBlocks = 2,
    /* BL is receiving the memory map of new firmware */
    Bootloader_State_ReceivingFirmwareMemoryMap = 3,
    /* BL is erasing physical blocks */
    Bootloader_State_ErasingPhysicalBlocks = 4,
    /* BL is flashing new firmware */
    Bootloader_State_DownloadingFirmware = 5,
    /* BL is receiving entry point/isr vector */
    Bootloader_State_ReceivingFirmwareMetadata = 6,
    /* Some error occured. //TODO make it more concrete */
    Bootloader_State_Error = 7,
    /* Bootloader is overwhelmed and has suspended the communication. */
    Bootloader_State_CommunicationStalled = 8,
};

enum Bootloader_WriteResult {
    /* Write was performed successfully. */
    Bootloader_WriteResult_Ok = 0,
    /* Requested address was not in any of the beforehand specified erasable pages */
    Bootloader_WriteResult_InvalidMemory = 1,
    /* Requested address has already been written */
    Bootloader_WriteResult_AlreadyWritten = 2,
    /* Write command timeouted */
    Bootloader_WriteResult_Timeout = 3,
};

/*
 * Configuration message exchanged between the flashing master and the bootloader. Has corresponding acknowledge.
 */
typedef struct Bootloader_Handshake_t {
	/* Which register is currently configured */
	enum Bootloader_Register	Register;

	/* Command to be carried out by the bootloader */
	enum Bootloader_Command	Command;

	/* Identifier of the targeted unit */
	enum Bootloader_BootTarget	Target;

	/* Value for selected register */
	uint32_t	Value;
} Bootloader_Handshake_t;


/*
 * Acknowledgement that a Handshake message has been received.
 * //TODO wastes bandwidth - remove reserved fields ASAP
 */
typedef struct Bootloader_HandshakeAck_t {
	/* Last written register */
	enum Bootloader_Register	Register;

	/* Identifier of the responding unit */
	enum Bootloader_BootTarget	Target;

	/* Error code indicating the result of last operation */
	enum Bootloader_HandshakeResponse	Response;

	/* Last written value */
	uint32_t	Value;
} Bootloader_HandshakeAck_t;


/*
 * Current bus master yields control of communication to the other node.
 * Sent between subtransactions.
 */
typedef struct Bootloader_CommunicationYield_t {
	/* Identifies targeted unit. */
	enum Bootloader_BootTarget	Target;
} Bootloader_CommunicationYield_t;


/*
 * Stream of data to be flashed into the MCU.
 */
typedef struct Bootloader_Data_t {
	/* Word aligned absolute address */
	uint32_t	Address;

	/* If set to true, only the lower 16 bits of Word will be flashed. */
	uint8_t	HalfwordAccess;

	/* Word to be written */
	uint32_t	Word;
} Bootloader_Data_t;

#define Bootloader_Data_Address_OFFSET	((float)0)
#define Bootloader_Data_Address_FACTOR	((float)4)
#define Bootloader_Data_Address_MIN	((float)0)

/*
 * Acknowledgement that the bootloader successfully received word of data. It is not necessary that the word has already been written.
 */
typedef struct Bootloader_DataAck_t {
	/* Word (4B) aligned address of last write comand. */
	uint32_t	Address;

	/* Identifies the result of previous write operation. */
	enum Bootloader_WriteResult	Result;
} Bootloader_DataAck_t;

#define Bootloader_DataAck_Address_OFFSET	((float)0)
#define Bootloader_DataAck_Address_FACTOR	((float)4)
#define Bootloader_DataAck_Address_MIN	((float)0)

/*
 * Request from the flash master to an ECU to exit the bootloader.
 */
typedef struct Bootloader_ExitReq_t {
	/* Which unit shall leave the bootloader. */
	enum Bootloader_BootTarget	Target;

	/* Force the bootloader to reset regardless of current state. Necessary when there is an ongoing transaction. */
	uint8_t	Force;

	/* Should the bootloader enter the application or only reboot itself? */
	uint8_t	InitializeApplication;
} Bootloader_ExitReq_t;


/*
 * Polls possible boot targets and detects their presence on the CAN bus. May request an ECU to enter the bootloader.
 */
typedef struct Bootloader_Ping_t {
	/* Identifies the unit targeted by this message. */
	enum Bootloader_BootTarget	Target;

	/* Shall the targeted unit reset into bootloader? */
	uint8_t	BootloaderRequested;
} Bootloader_Ping_t;


/*
 * Sent periodically by an active bootloader to announce its presence.
 */
typedef struct Bootloader_Beacon_t {
	/* Identifies which unit has active bootloader */
	enum Bootloader_BootTarget	Target;

	/* Current state of the bootloader */
	enum Bootloader_State	State;

	/* Why is the bootloader active? */
	enum Bootloader_EntryReason	EntryReason;

	/* Available flash size in kibibytes. */
	uint16_t	FlashSize;
} Bootloader_Beacon_t;

#define Bootloader_Beacon_FlashSize_OFFSET	((float)0)
#define Bootloader_Beacon_FlashSize_FACTOR	((float)1)
#define Bootloader_Beacon_FlashSize_MIN	((float)0)
#define Bootloader_Beacon_FlashSize_MAX	((float)4095)

/*
 * Targeted unit has received a Ping.
 */
typedef struct Bootloader_PingResponse_t {
	/* Identifies the unit which is currently transitioning to the bootloader. */
	enum Bootloader_BootTarget	Target;

	/* Targeted unit confirms entry request and schedules system reset. */
	uint8_t	BootloaderPending;

	/* True iff the application knows the bootloader metadata and hence the field BL_SoftwareBuild and BL_DirtyRepo contain valid values */
	uint8_t	BootloaderMetadataValid;

	/* True iff the bootloader has been flashed with dirty working tree */
	uint8_t	BL_DirtyRepo;

	/* First 8 hex digits of bootloader software build */
	uint32_t	BL_SoftwareBuild;
} Bootloader_PingResponse_t;


/*
 * Acknowledgement for a request to leave the bootloader.
 * Carries information, whether the unit can transition from the bootloader to the main firmware at the moment.
 * This transition is prohibited during the process of erasing or flashing. In that case only a power cycle or asserting the Force bit in ExitReq can cause the bootloader to quit.
 */
typedef struct Bootloader_ExitAck_t {
	/* Identifies the target unit */
	enum Bootloader_BootTarget	Target;

	/* True iff the unit is switching from the bootloader to firmware. */
	uint8_t	Confirmed;
} Bootloader_ExitAck_t;


/*
 * Information about the currently running software (SHA of git commit).
 */
typedef struct Bootloader_SoftwareBuild_t {
	/* First 8 hex digits of current git commit */
	uint32_t	CommitSHA;

	/* True iff the currently flashed code contains uncommited changes */
	uint8_t	DirtyRepo;

	/* Unit which sent this message */
	enum Bootloader_BootTarget	Target;
} Bootloader_SoftwareBuild_t;


void        candbInit              (void);

int Bootloader_decode_Handshake_s(const uint8_t* bytes, size_t length, Bootloader_Handshake_t* data_out);
int Bootloader_decode_Handshake(const uint8_t* bytes, size_t length, enum Bootloader_Register* Register_out, enum Bootloader_Command* Command_out, enum Bootloader_BootTarget* Target_out, uint32_t* Value_out);
int Bootloader_send_Handshake_s(const Bootloader_Handshake_t* data);
int Bootloader_get_Handshake(Bootloader_Handshake_t* data_out);
uint32_t Bootloader_Handshake_get_flags(void);
void Bootloader_Handshake_on_receive(int (*callback)(Bootloader_Handshake_t* data));
candb_bus_t Bootloader_Handshake_get_rx_bus(void);
int Bootloader_send_Handshake(enum Bootloader_Register Register, enum Bootloader_Command Command, enum Bootloader_BootTarget Target, uint32_t Value);
candb_bus_t Bootloader_Handshake_get_tx_bus(void);

int Bootloader_decode_HandshakeAck_s(const uint8_t* bytes, size_t length, Bootloader_HandshakeAck_t* data_out);
int Bootloader_decode_HandshakeAck(const uint8_t* bytes, size_t length, enum Bootloader_Register* Register_out, enum Bootloader_BootTarget* Target_out, enum Bootloader_HandshakeResponse* Response_out, uint32_t* Value_out);
int Bootloader_send_HandshakeAck_s(const Bootloader_HandshakeAck_t* data);
int Bootloader_get_HandshakeAck(Bootloader_HandshakeAck_t* data_out);
uint32_t Bootloader_HandshakeAck_get_flags(void);
void Bootloader_HandshakeAck_on_receive(int (*callback)(Bootloader_HandshakeAck_t* data));
candb_bus_t Bootloader_HandshakeAck_get_rx_bus(void);
int Bootloader_send_HandshakeAck(enum Bootloader_Register Register, enum Bootloader_BootTarget Target, enum Bootloader_HandshakeResponse Response, uint32_t Value);
candb_bus_t Bootloader_HandshakeAck_get_tx_bus(void);

int Bootloader_decode_CommunicationYield_s(const uint8_t* bytes, size_t length, Bootloader_CommunicationYield_t* data_out);
int Bootloader_decode_CommunicationYield(const uint8_t* bytes, size_t length, enum Bootloader_BootTarget* Target_out);
int Bootloader_send_CommunicationYield_s(const Bootloader_CommunicationYield_t* data);
int Bootloader_get_CommunicationYield(Bootloader_CommunicationYield_t* data_out);
uint32_t Bootloader_CommunicationYield_get_flags(void);
void Bootloader_CommunicationYield_on_receive(int (*callback)(Bootloader_CommunicationYield_t* data));
candb_bus_t Bootloader_CommunicationYield_get_rx_bus(void);
int Bootloader_send_CommunicationYield(enum Bootloader_BootTarget Target);
candb_bus_t Bootloader_CommunicationYield_get_tx_bus(void);

int Bootloader_decode_Data_s(const uint8_t* bytes, size_t length, Bootloader_Data_t* data_out);
int Bootloader_decode_Data(const uint8_t* bytes, size_t length, uint32_t* Address_out, uint8_t* HalfwordAccess_out, uint32_t* Word_out);
int Bootloader_send_Data_s(const Bootloader_Data_t* data);
int Bootloader_get_Data(Bootloader_Data_t* data_out);
uint32_t Bootloader_Data_get_flags(void);
void Bootloader_Data_on_receive(int (*callback)(Bootloader_Data_t* data));
candb_bus_t Bootloader_Data_get_rx_bus(void);
int Bootloader_send_Data(uint32_t Address, uint8_t HalfwordAccess, uint32_t Word);
candb_bus_t Bootloader_Data_get_tx_bus(void);

int Bootloader_decode_DataAck_s(const uint8_t* bytes, size_t length, Bootloader_DataAck_t* data_out);
int Bootloader_decode_DataAck(const uint8_t* bytes, size_t length, uint32_t* Address_out, enum Bootloader_WriteResult* Result_out);
int Bootloader_send_DataAck_s(const Bootloader_DataAck_t* data);
int Bootloader_get_DataAck(Bootloader_DataAck_t* data_out);
uint32_t Bootloader_DataAck_get_flags(void);
void Bootloader_DataAck_on_receive(int (*callback)(Bootloader_DataAck_t* data));
candb_bus_t Bootloader_DataAck_get_rx_bus(void);
int Bootloader_send_DataAck(uint32_t Address, enum Bootloader_WriteResult Result);
candb_bus_t Bootloader_DataAck_get_tx_bus(void);

int Bootloader_decode_ExitReq_s(const uint8_t* bytes, size_t length, Bootloader_ExitReq_t* data_out);
int Bootloader_decode_ExitReq(const uint8_t* bytes, size_t length, enum Bootloader_BootTarget* Target_out, uint8_t* Force_out, uint8_t* InitializeApplication_out);
int Bootloader_get_ExitReq(Bootloader_ExitReq_t* data_out);
uint32_t Bootloader_ExitReq_get_flags(void);
void Bootloader_ExitReq_on_receive(int (*callback)(Bootloader_ExitReq_t* data));
candb_bus_t Bootloader_ExitReq_get_rx_bus(void);

int Bootloader_decode_Ping_s(const uint8_t* bytes, size_t length, Bootloader_Ping_t* data_out);
int Bootloader_decode_Ping(const uint8_t* bytes, size_t length, enum Bootloader_BootTarget* Target_out, uint8_t* BootloaderRequested_out);
int Bootloader_get_Ping(Bootloader_Ping_t* data_out);
uint32_t Bootloader_Ping_get_flags(void);
void Bootloader_Ping_on_receive(int (*callback)(Bootloader_Ping_t* data));
candb_bus_t Bootloader_Ping_get_rx_bus(void);

int Bootloader_decode_Beacon_s(const uint8_t* bytes, size_t length, Bootloader_Beacon_t* data_out);
int Bootloader_decode_Beacon(const uint8_t* bytes, size_t length, enum Bootloader_BootTarget* Target_out, enum Bootloader_State* State_out, enum Bootloader_EntryReason* EntryReason_out, uint16_t* FlashSize_out);
int Bootloader_send_Beacon_s(const Bootloader_Beacon_t* data);
int Bootloader_get_Beacon(Bootloader_Beacon_t* data_out);
uint32_t Bootloader_Beacon_get_flags(void);
void Bootloader_Beacon_on_receive(int (*callback)(Bootloader_Beacon_t* data));
candb_bus_t Bootloader_Beacon_get_rx_bus(void);
bool Bootloader_Beacon_has_timed_out(void);
int Bootloader_send_Beacon(enum Bootloader_BootTarget Target, enum Bootloader_State State, enum Bootloader_EntryReason EntryReason, uint16_t FlashSize);
candb_bus_t Bootloader_Beacon_get_tx_bus(void);
bool Bootloader_Beacon_need_to_send(void);

int Bootloader_send_PingResponse_s(const Bootloader_PingResponse_t* data);
int Bootloader_send_PingResponse(enum Bootloader_BootTarget Target, uint8_t BootloaderPending, uint8_t BootloaderMetadataValid, uint8_t BL_DirtyRepo, uint32_t BL_SoftwareBuild);
candb_bus_t Bootloader_PingResponse_get_tx_bus(void);

int Bootloader_send_ExitAck_s(const Bootloader_ExitAck_t* data);
int Bootloader_send_ExitAck(enum Bootloader_BootTarget Target, uint8_t Confirmed);
candb_bus_t Bootloader_ExitAck_get_tx_bus(void);

int Bootloader_send_SoftwareBuild_s(const Bootloader_SoftwareBuild_t* data);
int Bootloader_send_SoftwareBuild(uint32_t CommitSHA, uint8_t DirtyRepo, enum Bootloader_BootTarget Target);
candb_bus_t Bootloader_SoftwareBuild_get_tx_bus(void);
bool Bootloader_SoftwareBuild_need_to_send(void);

#ifdef __cplusplus
}

template <typename T> bool        need_to_send   ();
template <typename T> candb_bus_t get_rx_bus     ();
template <typename T> bool        has_timed_out  ();
template <typename T> candb_bus_t get_tx_bus   ();

inline int send(const Bootloader_Handshake_t& data) {
    return Bootloader_send_Handshake_s(&data);
}

template <>
inline candb_bus_t get_rx_bus<Bootloader_Handshake_t>() {
    return Bootloader_Handshake_get_rx_bus();
}

template <>
inline candb_bus_t get_tx_bus<Bootloader_Handshake_t>() {
    return Bootloader_Handshake_get_tx_bus();
}

inline int send(const Bootloader_HandshakeAck_t& data) {
    return Bootloader_send_HandshakeAck_s(&data);
}

template <>
inline candb_bus_t get_rx_bus<Bootloader_HandshakeAck_t>() {
    return Bootloader_HandshakeAck_get_rx_bus();
}

template <>
inline candb_bus_t get_tx_bus<Bootloader_HandshakeAck_t>() {
    return Bootloader_HandshakeAck_get_tx_bus();
}

inline int send(const Bootloader_CommunicationYield_t& data) {
    return Bootloader_send_CommunicationYield_s(&data);
}

template <>
inline candb_bus_t get_rx_bus<Bootloader_CommunicationYield_t>() {
    return Bootloader_CommunicationYield_get_rx_bus();
}

template <>
inline candb_bus_t get_tx_bus<Bootloader_CommunicationYield_t>() {
    return Bootloader_CommunicationYield_get_tx_bus();
}

inline int send(const Bootloader_Data_t& data) {
    return Bootloader_send_Data_s(&data);
}

template <>
inline candb_bus_t get_rx_bus<Bootloader_Data_t>() {
    return Bootloader_Data_get_rx_bus();
}

template <>
inline candb_bus_t get_tx_bus<Bootloader_Data_t>() {
    return Bootloader_Data_get_tx_bus();
}

inline int send(const Bootloader_DataAck_t& data) {
    return Bootloader_send_DataAck_s(&data);
}

template <>
inline candb_bus_t get_rx_bus<Bootloader_DataAck_t>() {
    return Bootloader_DataAck_get_rx_bus();
}

template <>
inline candb_bus_t get_tx_bus<Bootloader_DataAck_t>() {
    return Bootloader_DataAck_get_tx_bus();
}

template <>
inline candb_bus_t get_rx_bus<Bootloader_ExitReq_t>() {
    return Bootloader_ExitReq_get_rx_bus();
}

template <>
inline candb_bus_t get_rx_bus<Bootloader_Ping_t>() {
    return Bootloader_Ping_get_rx_bus();
}

template <>
inline bool need_to_send<Bootloader_Beacon_t>() {
    return Bootloader_Beacon_need_to_send();
}

inline int send(const Bootloader_Beacon_t& data) {
    return Bootloader_send_Beacon_s(&data);
}

template <>
inline bool has_timed_out<Bootloader_Beacon_t>() {
    return Bootloader_Beacon_has_timed_out();
}

template <>
inline candb_bus_t get_rx_bus<Bootloader_Beacon_t>() {
    return Bootloader_Beacon_get_rx_bus();
}

template <>
inline candb_bus_t get_tx_bus<Bootloader_Beacon_t>() {
    return Bootloader_Beacon_get_tx_bus();
}

inline int send(const Bootloader_PingResponse_t& data) {
    return Bootloader_send_PingResponse_s(&data);
}

template <>
inline candb_bus_t get_tx_bus<Bootloader_PingResponse_t>() {
    return Bootloader_PingResponse_get_tx_bus();
}

inline int send(const Bootloader_ExitAck_t& data) {
    return Bootloader_send_ExitAck_s(&data);
}

template <>
inline candb_bus_t get_tx_bus<Bootloader_ExitAck_t>() {
    return Bootloader_ExitAck_get_tx_bus();
}

template <>
inline bool need_to_send<Bootloader_SoftwareBuild_t>() {
    return Bootloader_SoftwareBuild_need_to_send();
}

inline int send(const Bootloader_SoftwareBuild_t& data) {
    return Bootloader_send_SoftwareBuild_s(&data);
}

template <>
inline candb_bus_t get_tx_bus<Bootloader_SoftwareBuild_t>() {
    return Bootloader_SoftwareBuild_get_tx_bus();
}

#endif

#endif

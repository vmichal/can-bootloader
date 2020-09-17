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
enum { Bootloader_SoftwareBuild_id = 0x5FD };
enum { Bootloader_SerialOutput_id = 0x5FF };

enum Bootloader_BootTarget {
    /* Accumulator management System */
    Bootloader_BootTarget_AMS = 0,
};

/*
 * Request for an ECU to enter the bootloader.
 */
typedef struct Bootloader_EntryReq_t {
	/* Sequence counter, incremented each time a message is sent. */
	uint8_t	SEQ;

	/* Identifies the unit targeted by this message. */
	enum Bootloader_BootTarget	Target;
} Bootloader_EntryReq_t;

#define Bootloader_EntryReq_SEQ_OFFSET	((float)0)
#define Bootloader_EntryReq_SEQ_FACTOR	((float)1)
#define Bootloader_EntryReq_SEQ_MIN	((float)0)
#define Bootloader_EntryReq_SEQ_MAX	((float)64)

/*
 * Targeted unit has received a bootloader entry request and has reset pending.
 */
typedef struct Bootloader_EntryAck_t {
	/* Identifies the unit which is currently transitioning to the bootloader. */
	enum Bootloader_BootTarget	Target;
} Bootloader_EntryAck_t;


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

void candbInit(void);

int Bootloader_decode_EntryReq_s(const uint8_t* bytes, size_t length, Bootloader_EntryReq_t* data_out);
int Bootloader_decode_EntryReq(const uint8_t* bytes, size_t length, uint8_t* SEQ_out, enum Bootloader_BootTarget* Target_out);
int Bootloader_get_EntryReq(Bootloader_EntryReq_t* data_out);
void Bootloader_EntryReq_on_receive(int (*callback)(Bootloader_EntryReq_t* data));

int Bootloader_send_EntryAck_s(const Bootloader_EntryAck_t* data);
int Bootloader_send_EntryAck(enum Bootloader_BootTarget Target);

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

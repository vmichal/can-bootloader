#pragma once

#include "library/can.hpp"
#include "library/timer.hpp"
#include "library/pin.hpp"

extern Pin ledBlue;

// NOT thread-safe!

template <size_t queueLength>
class CANQueue {
public:
	CANQueue(CANPeriph* can, uint32_t interval)
			: can(can), interval(interval), readPos(0), writePos(0) {
	}

	void Init();

	// returns true if a message was sent
	bool Update();

	// all of these return false or NULL if the queue is full
	CANMessage_t* AllocateMessage();
	bool PushMessage(uint16_t sid, const uint8_t* data, size_t length);

private:
	CANPeriph* can;
	uint32_t interval;

	SysTickTimer intervalTimer;

	CANMessage_t queue[queueLength];
	uint16_t readPos, writePos;
};

template <size_t queueLength>
void CANQueue<queueLength>::Init() {
	intervalTimer.Restart();
}

template <size_t queueLength>
CANMessage_t* CANQueue<queueLength>::AllocateMessage() {
	auto newWritePos = (writePos + 1) % queueLength;

	if (newWritePos == readPos)
		return NULL;

	auto msg = &queue[writePos];
	writePos = newWritePos;
	return msg;
}

template <size_t queueLength>
bool CANQueue<queueLength>::PushMessage(uint16_t sid, const uint8_t* data, size_t length) {
	auto newWritePos = (writePos + 1) % queueLength;

	if (newWritePos == readPos)
		return false;

	queue[writePos].sid = sid;

	// memcpy is not worth the overhead here
	for (size_t i = 0; i < length; i++)
		queue[writePos].data[i] = data[i];
	// (unless it's fixed size)
	//memcpy(&queue[writePos].data, &data, 8);	// YOLO
	//*(uint64_t*)queue[writePos].data = *(uint64_t*)data; // MAXIMUM YOLO

	queue[writePos].dataLength = length;
	writePos = newWritePos;
	return true;
}

template <size_t queueLength>
bool CANQueue<queueLength>::Update() {
	if (readPos != writePos && intervalTimer.TimeElapsed(interval)) {
		//// if sending fails, throw the message away
		can->SendMessageIfBufferAvailable(&queue[readPos]);

		/*printf("CANQueue: Dispatch sid=%d data=", queue[readPos].sid);
		for (int i = 0; i < queue[readPos].dataLength; i++)
			printf("%02X ", queue[readPos].data[i]);
		printf("\r\n");*/

		readPos = (readPos + 1) % queueLength;
		intervalTimer.Restart();
		return true;
	}

	return false;
}

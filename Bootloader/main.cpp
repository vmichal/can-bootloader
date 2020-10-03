/*
 * eForce CAN Bootloader
 *
 * Written by Vojtech Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */

#include "main.hpp"
#include "bootloader.hpp"
#include "canmanager.hpp"
#include "tx2/tx.h"
#include "can_Bootloader.h"

#include <library/timer.hpp>
#include <BSP/can.hpp>
#include <BSP/gpio.hpp>
namespace boot {

	Bootloader bootloader;
	CanManager canManager{ bootloader };

	SysTickTimer lastBlueToggle;

	void setupCanCallbacks() {
		Bootloader_Data_on_receive([](Bootloader_Data_t* data) -> int {
			if (lastBlueToggle.RestartIfTimeElapsed(50_ms))
				gpio::LED_Blue_Toggle();

			std::uint32_t const address = data->Address << 2;

			WriteStatus const ret = data->HalfwordAccess
				? bootloader.write(address, static_cast<std::uint16_t>(data->Word))
				: bootloader.write(address, static_cast<std::uint32_t>(data->Word));

			canManager.SendDataAck(address, ret);

			return 0;
			});

		Bootloader_Handshake_on_receive([](Bootloader_Handshake_t* data) -> int {
			if (lastBlueToggle.RestartIfTimeElapsed(50_ms))
				gpio::LED_Blue_Toggle();

			Register const reg = regToReg(data->Register);
			auto const response = bootloader.processHandshake(reg, data->Value);

			canManager.SendHandshakeAck(reg, response, data->Value);
			return 0;
			});

		Bootloader_ExitReq_on_receive([](Bootloader_ExitReq_t* data) {
			if (bootloader.status() != Bootloader::Status::Ready) {
				canManager.SendExitAck(false);
				return 1;
			}
			canManager.SendExitAck(true);

			Bootloader::resetToApplication();
			return 0; //Not reached
			});

	}

	/*Transaction protocol:
	The communication uses messages Handshake and Data and their corresponding Acknowledgements
	For every sent message a corresponding ack must be awaited to make sure the system performed requested operation.
	Steps 1-7, 9-10 use the message Handshake. Step 8 uses the message Data
	1) The master must write 0x48656c69 to the TransactionMagic register
	2) The master sends the size of flashed binary
	3) The master sends the number of pages that will need to be erased
	4) [repeated] The master sends addresses of flash pages that need to be erased
	5) The master sends the entry point address
	6) The master sends the address of the interrupt service routine table
	7) Transaction magic must be sent by the master.

	8) The master sends pair of address and data word that will be written to the flash

	9) The master send checsum of the written firmware
	10) The master writes magic value to indicate the end of transaction
	*/

	/*TODO Big assumption is that the firmware is valid. We do not check this right now,
	because every CAN message has CRC of its own. We may check this in the future.*/

	void main() {

		[[maybe_unused]] auto const reason = explain_enter_reason(Bootloader::entryReason());
		[[maybe_unused]] auto const unitName = to_string(Bootloader::thisUnit);


		switch (Bootloader::entryReason()) {
		case EntryReason::ApplicationReturned:
			for (;;); // Latch after the application returned
			break;
		case EntryReason::backupRegisterCorrupted:
			break;
		case EntryReason::DontEnter:
			assert_unreachable();
			break;
		case EntryReason::EntryPointMismatch: {
			[[maybe_unused]] std::uint32_t const entry_point = reinterpret_cast<std::uint32_t const*>(jumpTable.interruptVector_)[1]; //second word of the interrupt table
			break;
		}
		case EntryReason::InvalidEntryPoint:
			break;
		case EntryReason::InvalidMagic:
			break;
		case EntryReason::Requested:
			break;
		case EntryReason::UnalignedInterruptVector:
			break;
		}

		txInit();
		gpio::LED_Orange_Off();

		setupCanCallbacks();
		bsp::can::enableIRQs(); //Enable reception from CAN

		for (;;) { //main loop

			if (static SysTickTimer t; t.RestartIfTimeElapsed(1_s))
				gpio::LED_Blue_Toggle();

			canManager.Update();

			txProcess();
		}

	}

	extern "C" [[noreturn]] void HardFault_Handler() {

		gpio::LED_Orange_On();

		for (;;) {
			BlockingDelay(100_ms);
			gpio::LED_Blue_Toggle();
		}

	}

}



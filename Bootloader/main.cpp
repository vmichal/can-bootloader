/*
 * eForce CAN Bootloader
 *
 * Written by Vojtìch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */

#include "main.hpp"
#include "bootloader.hpp"
#include "canmanager.hpp"
#include "tx2/tx.h"
#include "can_Bootloader.h"

#include <library/timer.hpp>
#include <BSP/gpio.hpp>
namespace boot {

	Bootloader bootloader;
	CanManager canManager{bootloader};

	void setupCanCallbacks() {
		Bootloader_Data_on_receive([](Bootloader_Data_t * data) -> int {
			std::uint32_t const address = data->Address << 2;

			WriteStatus const ret = data->HalfwordAccess 
				? bootloader.write(address, static_cast<std::uint16_t>(data->Word))
				: bootloader.write(address, static_cast<std::uint32_t>(data->Word));

			canManager.SendDataAck(address, ret);

			return 0;
			});

		Bootloader_Handshake_on_receive([](Bootloader_Handshake_t * data) -> int {
			Register const reg = regToReg(data->Register);
			auto const response = bootloader.processHandshake(reg, data->Value);

			canManager.SendHandshakeAck(reg, response, data->Value);
			return 0;
		});

		Bootloader_ExitReq_on_receive([](Bootloader_ExitReq_t * data) {
			debug_printf(("Exiting the bootloader.\r\n"));
			canManager.FlushSerialOutput();

			Bootloader::resetToApplication();
			return 0;
			});

	}

	/*Transaction protocol:
	The communication uses messages Handshake and Data and their corresponding Acknowledgements
	For every sent message a corresponding ack must be awaited to make sure the system performed requested operation.
	Steps 1-6 use the message Handshake. Step 7 uses the message Data
	1) The master must write 0x696c6548 to the TransactionMagic register
	2) The master sends the size of flashed binary
	3) The master sends the number of pages that will need to be erased
	4) [repeated] The master sends addresses of flash pages that need to be erased
	5) The master sends the entry point address
	6) The master sends the address of the interrupt service routine table

	7) The master sends pair of address and data word that will be written to the flash
	*/

	void main() {

		auto const reason = explain_enter_reason(Bootloader::entryReason());
		auto const unitName = to_string(Bootloader::thisUnit);

		debug_printf(("Entering bootloader of %s (%s)\r\n", unitName, reason));

		txInit();

		setupCanCallbacks();

		for (;;) { //main loop

			canManager.Update();

			txProcess();
		}

	}

	extern "C" [[noreturn]] void HardFault_Handler() {

		printf("HardFault\r\n");
		canManager.FlushSerialOutput();

		for (;;) {
			BlockingDelay(100_ms);
			gpio::LED_Blue_Toggle();
			gpio::LED_Orange_Toggle();
		}

	}

}



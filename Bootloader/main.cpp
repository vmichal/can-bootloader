/*
 * eForce CAN Bootloader
 *
 * Written by Vojtìch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */

#include "main.hpp"
#include "bootloader.hpp"
#include "tx2/tx.h"
#include "can_Bootloader.h"

namespace boot {

	Bootloader bootloader;

	void setupCanCallbacks() {
		Bootloader_Data_on_receive([](Bootloader_Data_t * data) -> int {
			std::uint32_t const address = data->Address << 2;

			if (data->HalfwordAccess)
				bootloader.write(address, static_cast<std::uint16_t>(data->Word));
			else
				bootloader.write(address, static_cast<std::uint32_t>(data->Word));

			return 0;

			});

		Bootloader_Handshake_on_receive([](Bootloader_Handshake_t * data) -> int {
			bootloader.handshake(static_cast<Register>(data->Register), data->Value);
			return 0;
		});
	}

	void main() {

		txInit();

		setupCanCallbacks();

		for (;;) { //main loop

			txProcess();



		}

	}

	extern "C" [[noreturn]] void HardFault_Handler() {

		for (;;) {

		}

	}

}



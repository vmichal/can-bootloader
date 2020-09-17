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
	CanManager canManager;

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
			Bootloader::resetToApplication();
			return 0;
			});

	}

	void main() {

		debug_printf(("Entering bootloader of %s\r\n", to_string(Bootloader::thisUnit)));

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



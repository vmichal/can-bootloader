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

	CanManager canManager;
	Bootloader bootloader{ canManager };

	namespace {

		void flushCAN(int bus) {
			assert(bus == bus_CAN1 || bus == bus_CAN2);

			//make sure all can messages have been transmitted
			if (bus == bus_CAN1)
				while (!ufsel::bit::all_set(CAN1->TSR, CAN_TSR_TME));
			else
				while (!ufsel::bit::all_set(CAN2->TSR, CAN_TSR_TME));
		}

		void setupCommonCanCallbacks() {

			Bootloader_ExitReq_on_receive([](Bootloader_ExitReq_t* data) {
				if (data->Target != Bootloader::thisUnit)
					return 2;

				if (data->Force) { //We want to restart the bootloader
					canManager.SendExitAck(true);
					flushCAN(Bootloader_ExitReq_status.bus);

					Bootloader::resetTo(BackupDomain::bootloader_magic);
				}

				if (bootloader.transactionInProgress()) {
					canManager.SendExitAck(false);
					return 1;
				}
				canManager.SendExitAck(true);
				flushCAN(Bootloader_ExitReq_status.bus);

				Bootloader::resetTo(BackupDomain::application_magic);
				});
		}

		void setupActiveCanCallbacks() {

			Bootloader_Data_on_receive([](Bootloader_Data_t* data) -> int {
				std::uint32_t const address = data->Address << 2;

				WriteStatus const ret = data->HalfwordAccess
					? bootloader.write(address, static_cast<std::uint16_t>(data->Word))
					: bootloader.write(address, static_cast<std::uint32_t>(data->Word));

				if (ret == WriteStatus::Ok)
					return 0;

				//TODO kill the transaction for now
				canManager.SendHandshake(handshake::abort);
				return 1;
				});

			Bootloader_DataAck_on_receive([](Bootloader_DataAck_t* data) -> int {
				//TODO implement for firmware dumping
				assert_unreachable();
				});

			Bootloader_Handshake_on_receive([](Bootloader_Handshake_t* data) -> int {
				Register const reg = static_cast<Register>(data->Register);
				auto const response = bootloader.processHandshake(reg, static_cast<Command>(data->Command), data->Value);

				canManager.SendHandshakeAck(reg, response, data->Value);
				return 0;
				});

			Bootloader_HandshakeAck_on_receive([](Bootloader_HandshakeAck_t* data) -> int {
				Bootloader_Handshake_t const last = canManager.lastSentHandshake();
				if (data->Register != last.Register || data->Value != last.Value)
					return 1;

				bootloader.processHandshakeAck(static_cast<HandshakeResponse>(data->Response));
				return 0;
				});

			Bootloader_CommunicationYield_on_receive([](Bootloader_CommunicationYield_t* const data) -> int {
				assert(data->Target == Bootloader::thisUnit); //TODO maybe allow multiple bootloaders on bus

				canManager.SendHandshake(bootloader.processYield());
				return 0;

				});
		}

		//Waits for some time (1 sec) to find out whether there is another BL on the CAN bus
		bool findOtherBootloaders() {
			Timestamp const entry_time = Timestamp::Now();

			while (!entry_time.TimeElapsed(otherBLdetectionTime)) {
				txProcess();

				if (Bootloader_get_Beacon(nullptr))
					return true;
			}
			return false;
		}
	}

	void main() {

		bsp::can::enableIRQs(); //Enable reception from CAN
		setupCommonCanCallbacks();

		//If the BL activation has been requested, we can assume that there is no other boi on the bus
		if (Bootloader::entryReason() != EntryReason::Requested && findOtherBootloaders())
			bootloader.enterPassiveMode();
		else
			setupActiveCanCallbacks();


		canManager.SendSoftwareBuild();
		canManager.SendBeacon(bootloader.status(), bootloader.entryReason());

		for (;;) { //main loop
			if (bootloader.isPassive() && Bootloader_get_Beacon(nullptr) == 0) { //Bootloader::Beacon timed out
				bootloader.exitPassiveMode();
				setupActiveCanCallbacks();
			}

			if (need_to_send<Bootloader_SoftwareBuild_t>())
				canManager.SendSoftwareBuild();

			if (need_to_send<Bootloader_Beacon_t>())
				canManager.SendBeacon(bootloader.status(), bootloader.entryReason());

			txProcess();
		}

	}

	extern "C" [[noreturn]] void HardFault_Handler() {

		for (;;) {
		}

	}

}



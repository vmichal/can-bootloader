/*
 * eForce CAN Bootloader
 *
 * Written by Vojtech Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */

#include <optional>

#include <API/BLdriver.hpp>

#include "bootloader.hpp"
#include "canmanager.hpp"
#include "tx2/tx.h"
#include "can_Bootloader.h"

#include <library/timer.hpp>
#include <BSP/can.hpp>
#include <BSP/gpio.hpp>
#include <BSP/timer.hpp>

namespace boot {

	CanManager canManager;
	Bootloader bootloader{ canManager };
	std::optional<Timestamp> lastReceivedData;


	namespace {

		void flushCAN(int bus, Duration timeout = 0_ms) {
			assert(bus == bus_CAN1 || bus == bus_CAN2);

			if (timeout == 0_ms)
				timeout = 3600_s; //an hour may be enough as "no timeout" :shrug:

			//make sure all can messages have been transmitted
			Timestamp const start = Timestamp::Now();
			if (bus == bus_CAN1)
				while (!start.TimeElapsed(timeout) && !ufsel::bit::all_set(CAN1->TSR, CAN_TSR_TME));
			else
				while (!start.TimeElapsed(timeout) && !ufsel::bit::all_set(CAN2->TSR, CAN_TSR_TME));
		}

		void setupCanCallbacks() {

			Bootloader_ExitReq_on_receive([](Bootloader_ExitReq_t* data) {
				if (data->Target != customization::thisUnit)
					return 2;

				std::uint16_t const destination = data->InitializeApplication
					? BackupDomain::application_magic : BackupDomain::bootloader_magic;

				if (!data->Force && bootloader.transactionInProgress()) {
					canManager.SendExitAck(false);
					return 1;
				}
				//We want to abort any ongoing transaction or no transaction in progress

				canManager.SendExitAck(true);
				flushCAN(get_rx_bus<Bootloader_ExitReq_t>(), 500_ms);

				resetTo(destination);
				});

			Bootloader_Handshake_on_receive([](Bootloader_Handshake_t* data) -> int {
				if (data->Target != customization::thisUnit)
					return 2;

				Register const reg = static_cast<Register>(data->Register);
				auto const response = bootloader.processHandshake(reg, static_cast<Command>(data->Command), data->Value);

				canManager.SendHandshakeAck(reg, response, data->Value);
				return 0;
				});

			Bootloader_HandshakeAck_on_receive([](Bootloader_HandshakeAck_t* data) -> int {
				if (data->Target != customization::thisUnit)
					return 2;

				Bootloader_Handshake_t const last = canManager.lastSentHandshake();
				if (data->Register != last.Register || data->Value != last.Value)
					return 1;

				bootloader.processHandshakeAck(static_cast<HandshakeResponse>(data->Response));
				return 0;
				});

			Bootloader_CommunicationYield_on_receive([](Bootloader_CommunicationYield_t* const data) -> int {
				if (data->Target != customization::thisUnit)
					return 2;

				canManager.SendHandshake(bootloader.processYield());
				return 0;

				});

			Bootloader_Data_on_receive([](Bootloader_Data_t* data) -> int {
				if (!bootloader.transactionInProgress())
					return 2; //The transaction has not started yet, this Data is not for us.

				std::uint32_t const address = data->Address << 2;

				lastReceivedData = Timestamp::Now();
				WriteStatus const ret = data->HalfwordAccess
					? bootloader.write(address, static_cast<std::uint16_t>(data->Word))
					: bootloader.write(address, static_cast<std::uint32_t>(data->Word));

				if (ret == WriteStatus::Ok || ret == WriteStatus::InsufficientData)
					return 0;
				else if (ret == WriteStatus::DiscontinuousWriteAccess) {
					auto const expectedWriteLocation = bootloader.expectedWriteLocation();
					assert(expectedWriteLocation.has_value());
					canManager.RestartDataFrom(*expectedWriteLocation);
					return 1;
				}

				//TODO kill the transaction for now
				canManager.SendHandshake(handshake::abort);
				return 1;
				});

			Bootloader_DataAck_on_receive([](Bootloader_DataAck_t* data) -> int {
				//TODO implement for firmware dumping
				return 0;
				});
		}
	}

	[[noreturn]]
	void main() {

		candbInit();

		setupCanCallbacks();

		canManager.SendSoftwareBuild();
		canManager.SendBeacon(bootloader.status(), Bootloader::entryReason());

		for (;;) { //main loop

			if (need_to_send<Bootloader_SoftwareBuild_t>())
				canManager.SendSoftwareBuild();

			if (need_to_send<Bootloader_Beacon_t>())
				canManager.SendBeacon(bootloader.stalled() ? Status::ComunicationStalled: bootloader.status(), bootloader.entryReason());

			txProcess();

			bool const some_data_received_long_time_ago = lastReceivedData.has_value() && lastReceivedData->TimeElapsed(1_s);
			if (bootloader.status() == Status::DownloadingFirmware && some_data_received_long_time_ago && !bootloader.stalled()) {
				if (static SysTickTimer lastRequest; lastRequest.RestartIfTimeElapsed(500_ms)) { //limit the frequency of requests
					auto const expectedAddress = bootloader.expectedWriteLocation();
					assert(expectedAddress.has_value());
					canManager.RestartDataFrom(*expectedAddress);
				}
			}

			if (txBufferGettingFull() && !bootloader.stalled()) {
				canManager.SendHandshake(handshake::stall);
				bootloader.stalled() = true;
			}

			if (bootloader.stalled() && txBufferGettingEmpty()) {
				canManager.SendHandshake(handshake::resume);
				bootloader.stalled() = false;
			}
		}

	}


	extern "C" [[noreturn]] void EverythingsFuckedUpHandler(bool const assertFailed) {
		Timestamp const hardfaultEntryTime = Timestamp::Now();

		for (;;) {
			if constexpr (boot::rebootAfterHardfault) {
				if (hardfaultEntryTime.TimeElapsed(boot::rebootDelayHardfault))
					resetTo(BackupDomain::bootloader_magic);
			}

			if (need_to_send<CarDiagnostics_RecoveryModeBeacon_t>()) {
				//Periodically notify the human that this ship is sinking.
				static std::uint8_t seq = 0;
				CarDiagnostics_RecoveryModeBeacon_t const msg {
						.ECU = CarDiagnostics_ECU_Bootloader,
						.ClockState = CarDiagnostics_ClockState_fully_operational,
						.NumFatalFirmwareErrors = 1,
						.FirmwareState = assertFailed ? CarDiagnostics_FirmwareState_failedAssertion
								: CarDiagnostics_FirmwareState_fatalError,
						.WillEnterBootloader = boot::rebootAfterHardfault,
						.SEQ = seq++
				};
				send(msg);

				//Append information about the software build to simplify the debugging
				canManager.SendBeacon(Status::Error, Bootloader::entryReason());
				canManager.SendSoftwareBuild();
			}

		}

	}

	extern "C" void HardFault_Handler() {
		EverythingsFuckedUpHandler(false);
	}
}



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

#include <ufsel/time.hpp>
#include <BSP/can.hpp>
#include <BSP/fdcan.hpp>
#include <BSP/gpio.hpp>

/*After consulting with Patrik, disassembly proved that this variable is initialized as part of .data init.
It is therefore available even before SystemInit is called and thus can be trusted to be valid before almost everything else.*/
//Variable counting system interrupts. Can be queried by HAL_GetTick()
__IO std::uint32_t SystemTicks = ufsel::time::systemStartTick;

namespace ufsel::time {
	Timestamp Timestamp::Now() {
		return Timestamp{SystemTicks};
	}
}

// TODO get rid of library. Timers are already in ufsel, move pin there as well possibly.

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
#if defined BOOT_STM32G4
			//if there are ack errors on the bus, there is no other unit, so flush is pointless
			constexpr auto has_pending_transmission = [](FDCAN_GlobalTypeDef * const can) { return !ufsel::bit::all_cleared(can->TXBRP, FDCAN_TXBRP_TRP_Msk); };

			// Wait until the peripheral transmits all messages (has_pending_transmission would go to false)
			// or the receiver is disconnected (has_ack_error would go to true) or the time runs out

			for (auto & b : bsp::can::bus_info) {
				if (b.candb_bus == bus) {
					FDCAN_GlobalTypeDef * const periph = b.get_peripheral();
					while (!start.TimeElapsed(timeout) && !bsp::can::has_ack_error(periph) && has_pending_transmission(periph));
				}
			}

#else
			if (bus == bus_CAN1)
				while (!start.TimeElapsed(timeout) && !ufsel::bit::all_set(CAN1->TSR, CAN_TSR_TME));
			else
				while (!start.TimeElapsed(timeout) && !ufsel::bit::all_set(CAN2->TSR, CAN_TSR_TME));
#endif
		}

		void setupRegularCanCallbacks() {

			Bootloader_ExitReq_on_receive([](Bootloader_ExitReq_t* data) {
				if (data->Target != customization::thisUnit)
					return 2;

				// The startup CAN check can be skipped in this case, because we have just received flash master's command to exit the bootloader.
				// It is therefore reasonable to assume that the master won't ask us to enter bootloader in 50 ms
				BackupDomain::magic const destination = data->InitializeApplication
					? BackupDomain::magic::app_skip_can_check : BackupDomain::magic::bootloader;

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

			// Respond to pings as well
			Bootloader_Ping_on_receive([](Bootloader_Ping_t * ping) -> int {
				if (ping->Target != customization::thisUnit)
					return 2;
				// TODO Do not respond to pings for now, only send beacon and software build
				canManager.SendSoftwareBuild();
				canManager.SendBeacon(bootloader.stalled() ? Status::ComunicationStalled: bootloader.status(), bootloader.entryReason());

				return 0;

				canManager.SendPingResponse(ping->BootloaderRequested);
				return 0;
			});
		}

		void switchFromStartupCheckToRegularOperation() {
			//Only executable during startup check
			assert(Bootloader::entryReason() == EntryReason::StartupCanBusCheck);

			Bootloader_Ping_on_receive(nullptr); // Deregister callback for Bootloader::Ping
			setupRegularCanCallbacks(); // Register new callbacks for proper operation
			Bootloader::setEntryReason(EntryReason::Requested); // Inform the bootloader that regular operation shall be initiated
		}

		void setupStartupCheckCanCallbacks() {
			// Callbacks for messages received during startup CAN bus check.
			Bootloader_Ping_on_receive([](Bootloader_Ping_t * ping) -> int {
				if (ping->Target != customization::thisUnit)
					return 2;

				canManager.SendPingResponse(ping->BootloaderRequested);
				if (ping->BootloaderRequested)
					switchFromStartupCheckToRegularOperation();
				return 0;
			});
		}
	}

	[[noreturn]]
	void main() {

		candbInit();

		if (bootloader.entryReason() == EntryReason::StartupCanBusCheck)
			setupStartupCheckCanCallbacks();
		else
			setupRegularCanCallbacks();

		for (;;) { //main loop
			txProcess();

			if (bootloader.startupCheckInProgress()) {
				if (systemStartupTime.TimeElapsed() > customization::startupCanBusCheckDuration) {
					// Enough time elapsed without receiving any BL request. Start the application
					resetTo(BackupDomain::magic::app_skip_can_check);
				}
				continue;
				// Prevent the rest of main loop from executing when we are in the CAN bus startup check
			}

			if (need_to_send<Bootloader_SoftwareBuild_t>())
				canManager.SendSoftwareBuild();
			if (need_to_send<Bootloader_Beacon_t>())
				canManager.SendBeacon(bootloader.stalled() ? Status::ComunicationStalled: bootloader.status(), bootloader.entryReason());

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
			if (ufsel::bit::all_set(SysTick->CTRL, SysTick_CTRL_TICKINT_Msk)) {
				// When invoked from HF handler, SysTick interrupt would never occur
				// This way time keeping is preserved at all times
				// TODO consider disabling interrupts
				ufsel::bit::clear(std::ref(SysTick->CTRL), SysTick_CTRL_TICKINT_Msk);
				++SystemTicks;
			}

			if constexpr (boot::rebootAfterHardfault) {
				if (hardfaultEntryTime.TimeElapsed(boot::rebootDelayHardfault))
					resetTo(BackupDomain::magic::bootloader);
			}

			if (need_to_send<Bootloader_Beacon_t>()) {
				//Periodically notify the human that this ship is sinking.

				canManager.SendBeacon(Status::Error, Bootloader::entryReason());
				//Append information about the software build to simplify the debugging
				canManager.SendSoftwareBuild();
			}

		}

	}

	extern "C" void HardFault_Handler() {
		EverythingsFuckedUpHandler(false);
	}
}

namespace ufsel::assertion {

	// Provide implementation of handlers for ufsel::assert
	[[noreturn]] void assertionFailedHandler(char const * const file, char const * const function, int line) {
		boot::EverythingsFuckedUpHandler(true);
	}

	[[noreturn]] void unreachableCodeHandler(char const * const file, char const * const function, int line) {
		boot::EverythingsFuckedUpHandler(true);
	}
}


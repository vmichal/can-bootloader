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
	Bootloader bootloader {canManager};

	SysTickTimer lastBlueToggle;

	void setupCanCallbacks() {
		Bootloader_Data_on_receive([](Bootloader_Data_t* data) -> int {
			std::uint32_t const address = data->Address << 2;

			WriteStatus const ret = data->HalfwordAccess
				? bootloader.write(address, static_cast<std::uint16_t>(data->Word))
				: bootloader.write(address, static_cast<std::uint32_t>(data->Word));

			canManager.SendDataAck(address, ret);

			return 0;
			});

		Bootloader_Handshake_on_receive([](Bootloader_Handshake_t* data) -> int {
			Register const reg = static_cast<Register>(data->Register);
			auto const response = bootloader.processHandshake(reg, static_cast<Command>(data->Command), data->Value);

			canManager.SendHandshakeAck(reg, response, data->Value);
			return 0;
			});

		Bootloader_ExitReq_on_receive([](Bootloader_ExitReq_t* data) {
			if (!data->Force && bootloader.status() != Status::Ready) {
				canManager.SendExitAck(false);
				return 1;
			}
			canManager.SendExitAck(true);

			Bootloader::resetToApplication();
			return 0; //Not reached
			});

		Bootloader_HandshakeAck_on_receive([](Bootloader_HandshakeAck_t * data) -> int {
			Bootloader_Handshake_t const last = canManager.lastSentHandshake();
			if (data->Register != last.Register || data->Value != last.Value)
				return 1;

			bootloader.processHandshakeAck(static_cast<HandshakeResponse>(data->Response));
			return 0;
			});

		Bootloader_CommunicationYield_on_receive([](Bootloader_CommunicationYield_t * const data) -> int {
			assert(data->Target == Bootloader::thisUnit); //TODO maybe allow multiple bootloaders on bus

			bootloader.processYield();
			return 0;
			
			});
	}

	/*Transaction protocol:


	=== Transaction magic ===
	The value of transaction magic is 0x696c6548 (the result of little-endian 32bit access on a string buffer containing the string "Heli")

	=== Physical and logical memory blocks ===
	The map of available physical memory blocks is the boot master's abstraction to underlying hardware.
	Different flash architectures are structured into pages of the same size (f105), or into several big sectors with variable size (f412).
	I'll call these two entities collectively 'physical memory blocks' to underline the distinction from 'logical memory blocks' that may span multiple physical memory blocks.

	=== Transaction overview ===
	Steps in the description further will be prefixed by either 'H' or 'D' to make clear whether that step is performed using the message 'Handshake' or 'Data'.
	The communication uses messages Handshake and Data and their corresponding Acknowledgements
	For every sent message a corresponding ack must be awaited to make sure the system performed requested operation.

	Prerequisite: Bootloader is in state Ready. If it's not (e.g. because previous transaction crashed) it must be reset via the message ExitReq with bit Force == 1)

	=== Initialization ===
		1) H: The master writes to the TransactionMagic register
		2) H: The master chooses the type of transaction.

	=== If the chosen transaction type is FLASHING ===
		3)     Map of available physical memory blocks is transmitted by the bootloader (see 'Transmission of available physical memory' below)
		4)     Firmware memory map is transmitted by the master  (see 'Transmission of logical memory map' below)
		5)     Physical memory blocks are chosen and erased  (see 'Physical memory block erassure' below)
		6)     Firmware is downloaded (see 'Firmwarer download' below)
		7)     Bootloader receives firmware metadata (see 'Firmware metadata' below)
		8)  H: The master writes to the TransactionMagic to indicate the end of transaction


	=== Transmission of available physical memory ===
	The bootloader may need to inform the flash master about the physical memory map of the target device. It shall be carried out this way
		1) H: The bootloader transmits the transaction magic to indicate the start of subtransaction
		2) H: The bootloader transmits the number of available physical memory blocks
		Repeat steps 3-4 n times, where n is the number sent in step 2:
			3) H: The bootloader transmits the starting address of next physical memory block
			4) H: The bootloader transmits the size of next physical memory block
		5) H: The bootloader transmits the transaction magic to indicate end of subtransaction


	=== Transmission of logical memory map ===
	The flash master informs the bootloader about the logical memory map of the application firmware. It shall be carried out this way:
		1) H: The master transmits the transaction magic to indicate the start of subtransaction
		2) H: The master transmits the number of logical memory blocks
		Repeat steps 3-4 n times, where n is the number sent in step 2:
			3) H: The master transmits the starting address of next logical memory block
			4) H: The master transmits the size of next logical memory block
		5) H: The master commands the bootloader to verify the specified logical memory map
		6) H: The bootloader confirms by writing to the TransactionMagic register that the received firmware memory map is valid and can be flashed.
		7) H: The master transmits the transaction magic to indicate end of subtransaction

	=== Physical memory block erassure ===
	The flash master commands the bootloader to erase chosen flash pages/sectors. This way both sides know precisely which blocks of memory are available
	and may perform sanity and transaction integrity checks.
	This subtransaction shall be carried out this way:
		1) H: The master transmits the transaction magic to indicate the start of subtransaction
		2) H: The master transmits the number of physical memory blocks to erase
		Repeat step 3 n times, where n is the number sent in step 2:
			3) H: The master transmits the starting address of next physical memory block
		4) H: The master commands the bootloader to erase specified physical memory blocks
		5) H: The bootloader confirms by writing to the TransactionMagic register that the erassure is finished.
		6) H: The master transmits the transaction magic to indicate end of subtransaction


	=== Firmware download ===
	The flash master sends words of firmware binary one by one to the bootloader.
	This subtransaction shall be carried out this way:
		1) H: The master transmits the transaction magic to indicate the start of subtransaction
		2) H: The master sends the size of flashed binary
		Repeat step 3 for each word/half word of the firmware binary:
			3) D: The master transmits an address together with word of data. The bootloader writes this word to specified address
		4) H: The master send checksum of the written firmware
		5) H: The master transmits the transaction magic to indicate end of subtransaction




	=== Firmware metadata ===
	The flash master tells the bootloader, where to find the firmware's entry point and isr vector
	This subtransaction shall be carried out this way:
		1) H: The master transmits the transaction magic to indicate the start of subtransaction
		2) H: The master sends the entry point address
		3) H: The master sends the address of the interrupt service routine table address
		4) H: The master transmits the transaction magic to indicate end of subtransaction



	*/

	void main() {

		txInit();

		setupCanCallbacks();
		bsp::can::enableIRQs(); //Enable reception from CAN

		for (;;) { //main loop

			canManager.Update(bootloader);

			txProcess();
		}

	}

	extern "C" [[noreturn]] void HardFault_Handler() {

		for (;;) {
		}

	}

}



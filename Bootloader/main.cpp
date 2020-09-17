/*
 * eForce CAN Bootloader
 *
 * Written by Vojtìch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */

#include "main.hpp"
#include "bootloader.hpp"

namespace boot {


	void main() {

	}

	extern "C" [[noreturn]] void HardFault_Handler() {

		for (;;) {

		}

	}

}



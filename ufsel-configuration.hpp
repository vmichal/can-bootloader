/*
 * eForce CAN Bootloader
 *
 * Written by Vojtěch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 *
 * This header file contains configuration of the UFSEL library
 */
#pragma once

#include <cstdint>


extern "C" [[noreturn]] void HardFault_Handler();

namespace ufsel {


	namespace bit {
		using machine_word = std::uint32_t;
	}


	namespace assertion {
		constexpr bool enableAssert = true;
		constexpr bool breakInFailedAssert = false;

		[[noreturn]] inline void assertionFailedHandler() {
			HardFault_Handler();
		}

		[[noreturn]] inline void unreachableCodeHandler() {
			HardFault_Handler();
		}
	}
}

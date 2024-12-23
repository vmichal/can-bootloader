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

namespace ufsel {

	namespace time {
		//Initial value of the SystemTicks variable which stores value for Timestamp:now()
		constexpr std::uint32_t systemStartTick = 0xcafe'babe;
	}

	namespace bit {
		using machine_word = std::uint32_t;
	}


	namespace assertion {
		/*Turns on and off assert functionality.

		If false, all calls to assert() compile into a noop and thus have no influence on the program
		If true, family of assert functions validate their argument and should an error be detected, SW breakpoint is hit. */
		constexpr bool enableAssert = true;

		/*If set to true, debugging experience is enhanced by breaking in the body of assert_fun when the tested condition fails.
		False is the safer option, because failed assertion will lead straight to HF handler and hence safe state will be guaranteed.*/
		constexpr bool breakInFailedAssert = false;

		[[noreturn]] void assertionFailedHandler(char const * file, char const * function, int line);

		[[noreturn]] void unreachableCodeHandler(char const * file, char const * function, int line);
	}
}

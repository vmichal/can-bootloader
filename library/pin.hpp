#pragma once

#include <cstdint>
#include <Bootloader/options.hpp>




enum class PinMode { //These two mode are sufficient to control CAN peripheral.
#ifdef BOOT_STM32F1
	input_floating = 0b0100,
	af_pushpull = 0b1010
#else 
#ifdef BOOT_STM32F4
	alternate_function
#else
#error MCU not specified!
#endif
#endif
};

struct Pin {
	constexpr Pin(std::uintptr_t gpio, uint16_t mask, PinMode mode) : address(gpio), pin(mask), mode_{mode} {}

	GPIO_TypeDef* gpio() const {
		return reinterpret_cast<GPIO_TypeDef*>(address);
	}

	std::uintptr_t const address;
	uint8_t const pin;
	PinMode mode_;
};

#pragma once

#include <stm32f10x.h>
#include <cstdint>


enum class PinMode { //These two mode are sufficient to control CAN peripheral.
	//TODO fill this with relevant data from your reference manual
	input_floating = 0b0100,
	af_pushpull = 0b1010
};

struct Pin {
	constexpr Pin(std::uintptr_t gpio, uint16_t mask, PinMode mode) : address(gpio), pin(mask), mode_{mode} {}

	GPIO_TypeDef* gpio() const {
		return reinterpret_cast<GPIO_TypeDef*>(address);
	}

	void Set() const { //TODO make not const as soon as constinit replaces constexpr in file gpio.hpp
		gpio()->BSRR = pin;
	}

	void Clear() const {
		gpio()->BRR = pin;
	}

	void Toggle() {
		gpio()->ODR ^= pin;
	}

	uint16_t Read() const {
		return gpio()->IDR & pin;
	}

	void Write(uint32_t value) {
		if (value)
			Set();
		else
			Clear();
	}

	std::uintptr_t const address;
	uint16_t const pin;
	PinMode mode_;
};

template <unsigned int bits, typename ShiftReg_t = uint32_t>
class FilteredPin {
public:
	ShiftReg_t const mask = (1 << bits) - 1;

	FilteredPin(Pin const& pin) : pin(pin), shiftReg(0) {}

	bool IsAllSet() const volatile { return shiftReg == mask; }
	bool IsAllCleared() const volatile { return shiftReg == 0; }

	bool IsAnySet() const volatile { return shiftReg != 0; }
	bool IsAnyCleared() const volatile { return shiftReg != mask; }

	void Sample() volatile {
		shiftReg = ((shiftReg << 1) | (pin.Read() ? 1 : 0)) & mask;
	}

	ShiftReg_t GetShiftReg() const volatile {
		return shiftReg;
	}

private:
	Pin const& pin;
	ShiftReg_t shiftReg;
};

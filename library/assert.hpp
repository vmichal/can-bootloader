
#ifndef ASSERT_H
#define ASSERT_H

#include <Bootloader/options.hpp>

extern "C" [[noreturn]] void HardFault_Handler();

inline bool assert_fun(const bool condition, const char* const where, const char* const func, int line) {
	if constexpr (enableAssert) {
		if (!condition) {
			__asm("BKPT");
			HardFault_Handler();
		}
	}
	return condition;
}



[[noreturn]]
inline void assert_unreachable_fun(const char * const where, const char * const func, int line) {
	if constexpr (enableAssert) {
		__asm("BKPT");
	}
	HardFault_Handler();
}

#define assert_unreachable() assert_unreachable_fun(__FILE__, __func__, __LINE__)
#define assert(cond) assert_fun(cond, __FILE__, __func__, __LINE__)



#endif

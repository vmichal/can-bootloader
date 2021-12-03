
#include <cstdint>
#include "isr_vector.hpp"

extern "C" {

	extern std::uint32_t _estack[];

	__attribute__((section(".isr_vector"))) interrupt_service_routine interrupt_routines[]{
		reinterpret_cast<interrupt_service_routine>(_estack),
		Reset_Handler,
		NMI_Handler,
		HardFault_Handler,
		MemManage_Handler,
		BusFault_Handler,
		UsageFault_Handler,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		SVC_Handler,
		DebugMon_Handler,
		nullptr,
		PendSV_Handler,
		SysTick_Handler,
		IRQ_HANDLERS
	};

	void Default_Handler() {
		HardFault_Handler();
	}

}

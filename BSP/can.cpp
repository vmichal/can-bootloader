#include "can.hpp"
#include "timer.hpp"

#include <cstring>

#include <ufsel/bit_operations.hpp>


#include <library/assert.hpp>
#include <CANdb/tx2/tx.h>
#include <CANdb/can_Bootloader.h>

namespace bsp::can {

	void enableIRQs() {

		CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);
		CAN_ITConfig(CAN2, CAN_IT_FMP0, ENABLE);
	}

	void Initialize(void) {
		CAN_InitTypeDef CAN_InitStructure;

		//enable peripheral clock to CAN1, CAN2
		ufsel::bit::set(std::ref(RCC->APB1ENR), RCC_APB1ENR_CAN1EN, RCC_APB1Periph_CAN2);

		/* CAN cell init */
		CAN_InitStructure.CAN_TTCM = DISABLE;
		CAN_InitStructure.CAN_ABOM = ENABLE;
		CAN_InitStructure.CAN_AWUM = DISABLE;
		CAN_InitStructure.CAN_NART = DISABLE;
		CAN_InitStructure.CAN_RFLM = DISABLE;
		CAN_InitStructure.CAN_TXFP = ENABLE;
		CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;

		/* CAN Baudrate */
		CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;
		CAN_InitStructure.CAN_BS1 = CAN_BS1_3tq;
		CAN_InitStructure.CAN_BS2 = CAN_BS2_2tq;

		constexpr Frequency APB1_frequency = 36'000'000_Hz;
		constexpr int quanta_per_bit = 6;

		CAN_InitStructure.CAN_Prescaler = APB1_frequency / can1_frequency / quanta_per_bit;
		CAN_Init(CAN1, &CAN_InitStructure);

		CAN_InitStructure.CAN_Prescaler = APB1_frequency / can2_frequency / quanta_per_bit;
		CAN_Init(CAN2, &CAN_InitStructure);

		//11 bits standard IDs. They share prefix 0x62_, the last nibble is variable
		constexpr unsigned sharedPrefix = 0x62 << 4;
		constexpr unsigned mustMatch = ufsel::bit::bitmask_of_width(7) << 4;

		using namespace ufsel;

		//All filters must be accessed via CAN1, because
		//[ref manual f105 24.9.5] In connectivity line devices, the registers from offset 0x200 to 31C are present only in CAN1.
		bit::set(std::ref(CAN1->FMR), CAN_FMR_FINIT);

		//Configure filters in mask mode
		bit::modify(std::ref(CAN1->FM1R), bit::bitmask_of_width(2), 0);

		bit::modify(std::ref(CAN1->FS1R), bit::bitmask_of_width(2), 0b11); //make filters 32bits wide


		bit::modify(std::ref(CAN1->FFA1R), bit::bitmask_of_width(2), 0); //assign filters to FIFO 0

		//activate filters (they can still be modified, because FINIT flag overrides this)
		bit::modify(std::ref(CAN1->FA1R), bit::bitmask_of_width(2), 0b11);

		CAN1->sFilterRegister[0].FR1 = CAN1->sFilterRegister[1].FR1 = sharedPrefix << (5 + 16);
		CAN1->sFilterRegister[0].FR2 = CAN1->sFilterRegister[1].FR2 = mustMatch << (5 + 16);

		//Each CAN peripheral gets only one filter.
		bit::modify(std::ref(CAN1->FMR), bit::bitmask_of_width(6), 1, 8);
		bit::clear(std::ref(CAN1->FMR), CAN_FMR_FINIT);


		NVIC_EnableIRQ(CAN1_RX0_IRQn);
		NVIC_EnableIRQ(CAN2_RX0_IRQn);
	}
}

/* Implementation of function required by tx library.
   These functions must use C linkage, because they are used by the code generated from CANdb (which is pure C). */

constexpr auto bus_connected_to_CAN1 = bus_CAN1;
constexpr auto bus_connected_to_CAN2 = bus_CAN2;


static_assert(bus_connected_to_CAN1 != bus_UNDEFINED);
static_assert(bus_connected_to_CAN2 != bus_UNDEFINED);

extern "C" {
	uint32_t txGetTimeMillis() {
		return (Timestamp::Now() - SystemTimer::bootTime).toMilliseconds();
	}

	int txHandleCANMessage(uint32_t timestamp, int bus, CAN_ID_t id, const void* data, size_t length) {
		return 0;
	}

	int txSendCANMessage(int const bus, CAN_ID_t const id, const void* const data, size_t const length) {
		assert(bus == bus_connected_to_CAN1 || bus == bus_connected_to_CAN2);

		CanTxMsg TxMessage;

		TxMessage.StdId = id;
		TxMessage.ExtId = 0;
		TxMessage.RTR = CAN_RTR_DATA;
		TxMessage.IDE = CAN_ID_STD;
		TxMessage.DLC = length;
		memcpy(TxMessage.Data, data, length);

		CAN_TypeDef* const periph = bus == bus_connected_to_CAN1 ? CAN1 : CAN2;
		bool const success = CAN_Transmit(periph, &TxMessage) != CAN_TxStatus_NoMailBox;
		return success ? 0 : -TX_SEND_BUFFER_OVERFLOW;
	}
}

extern "C" void CAN1_RX0_IRQHandler(void)
{
	CanRxMsg RxMessage;
	CAN_Receive(CAN1, CAN_FIFO0, &RxMessage);

	if (RxMessage.IDE == CAN_Id_Standard) {
		if (txReceiveCANMessage(bus_connected_to_CAN1, RxMessage.StdId, RxMessage.Data, RxMessage.DLC) < 0) {
			// ERROR
		}
	}
}

extern "C" void CAN2_RX0_IRQHandler(void)
{
	CanRxMsg RxMessage;

	CAN_Receive(CAN2, CAN_FIFO0, &RxMessage);

	if (RxMessage.IDE == CAN_Id_Standard) {
		if (txReceiveCANMessage(bus_connected_to_CAN2, RxMessage.StdId, RxMessage.Data, RxMessage.DLC) < 0) {
			// ERROR
		}
	}
}




/* END OF FILE */

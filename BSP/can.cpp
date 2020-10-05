#include "can.hpp"
#include "timer.hpp"

#include <cstring>


#include <CANdb/tx2/tx.h>
#include <CANdb/can_Bootloader.h>

namespace bsp::can {

	void enableIRQs() {

		CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);
		CAN_ITConfig(CAN2, CAN_IT_FMP0, ENABLE);
	}

	void disableIRQs() {
		CAN_ITConfig(CAN1, CAN_IT_FMP0, DISABLE);
		CAN_ITConfig(CAN2, CAN_IT_FMP0, DISABLE);
	}

	void Initialize(void) {
		CAN_InitTypeDef CAN_InitStructure;
		CAN_FilterInitTypeDef  CAN_FilterInitStructure;
		NVIC_InitTypeDef NVIC_InitStruct;

		RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN2, ENABLE);

		/* CAN register init */
		CAN_DeInit(CAN1);
		CAN_StructInit(&CAN_InitStructure);

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

		/* CAN register init */
		CAN_DeInit(CAN2);
		CAN_StructInit(&CAN_InitStructure);

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

		CAN_InitStructure.CAN_Prescaler = APB1_frequency / can2_frequency / quanta_per_bit;

		CAN_Init(CAN2, &CAN_InitStructure);

		/* CAN filter init */
		CAN_FilterInitStructure.CAN_FilterNumber = 0;
		CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;
		CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_32bit;
		CAN_FilterInitStructure.CAN_FilterIdHigh = 0x0000;
		CAN_FilterInitStructure.CAN_FilterIdLow = 0x0000;
		CAN_FilterInitStructure.CAN_FilterMaskIdHigh = 0x0000;
		CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x0000;
		CAN_FilterInitStructure.CAN_FilterFIFOAssignment = 0;
		CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
		CAN_FilterInit(&CAN_FilterInitStructure);

		CAN_FilterInitStructure.CAN_FilterNumber = 1;
		CAN_FilterInit(&CAN_FilterInitStructure);

		CAN_SlaveStartBank(1);

		NVIC_InitStruct.NVIC_IRQChannel = CAN1_RX0_IRQn;
		NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
		NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0;
		NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;
		NVIC_Init(&NVIC_InitStruct);

		NVIC_InitStruct.NVIC_IRQChannel = CAN2_RX0_IRQn;
		NVIC_Init(&NVIC_InitStruct);

	}
}

/* Implementation of function required by tx library.
   These functions must use C linkage, because they are used by the code generated from CANdb (which is pure C). */

constexpr auto bus_connected_to_CAN1 = bus_CAN1;
constexpr auto bus_connected_to_CAN2 = bus_CAN2;



extern "C" {
	uint32_t txGetTimeMillis() {
		return (Timestamp::Now() - SystemTimer::bootTime).toMilliseconds();
	}

	int txHandleCANMessage(uint32_t timestamp, int bus, CAN_ID_t id, const void* data, size_t length) {
		return 0;
	}

	int txSendCANMessage(int const bus, CAN_ID_t const id, const void* const data, size_t const length) {

		if (bus != bus_connected_to_CAN1 && bus != bus_connected_to_CAN2)
			return -TX_NOT_IMPLEMENTED;

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

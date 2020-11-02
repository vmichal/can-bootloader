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

	namespace {

		void peripheralRequestInitialization(CAN_TypeDef& can) {
			using namespace ufsel;

			//exit sleep mode and enter initialization
			std::uint32_t const copy = bit::clear(can.MCR, CAN_MCR_SLEEP);
			can.MCR = bit::set(copy, CAN_MCR_INRQ);
		}

		//wait for bxCAN to synchronize with the bus (leave initialization)
		void peripheralAwaitSynchronization(CAN_TypeDef& can) {
			for (; ufsel::bit::all_set(can.MSR, CAN_MSR_INAK););
		}

		void peripheralInit(CAN_TypeDef& can, unsigned prescaler) {
			using namespace ufsel;
			//await acknowledge that the peripheral entered initialization mode
			for (; bit::all_cleared(can.MSR, CAN_MSR_INAK););

			constexpr bool ttcm = false,
				abom = true,
				awum = false,
				nart = false, //Maybe make this dynamic - start requreing ACK frames when transaction starts
				rflm = false,
				txfp = true;

			constexpr unsigned sjw = 1, bs1 = 3, bs2 = 2;
			constexpr bool silent = false, loopback = false;

			bit::set(std::ref(can.MCR),
				ttcm ? CAN_MCR_TTCM : 0,
				abom ? CAN_MCR_ABOM : 0,
				awum ? CAN_MCR_AWUM : 0,
				nart ? CAN_MCR_NART : 0,
				rflm ? CAN_MCR_RFLM : 0,
				txfp ? CAN_MCR_TXFP : 0);

			can.BTR = bit::bitmask(
				silent ? CAN_BTR_SILM : 0,
				loopback ? CAN_BTR_LBKM : 0,
				(sjw - 1) << 24,
				(bs2 - 1) << 20,
				(bs1 - 1) << 16,
				(prescaler - 1)
			);

			//request to enter normal mode
			bit::clear(std::ref(can.MCR), CAN_MCR_INRQ);
		}
	}

	void Initialize(void) {
		using namespace ufsel;

		//enable peripheral clock to CAN1, CAN2
		bit::set(std::ref(RCC->APB1ENR), RCC_APB1ENR_CAN1EN, RCC_APB1Periph_CAN2);
		//wake up both peripherals and send them to initialization mode
		peripheralRequestInitialization(*CAN1);
		peripheralRequestInitialization(*CAN2);

		constexpr Frequency APB1_frequency = 36'000'000_Hz;
		constexpr int quanta_per_bit = 6;

		//Initialize peripherals
		peripheralInit(*CAN1, APB1_frequency / can1_frequency / quanta_per_bit);
		peripheralInit(*CAN2, APB1_frequency / can2_frequency / quanta_per_bit);

		//11 bits standard IDs. They share prefix 0x62_, the last nibble is variable
		constexpr unsigned sharedPrefix = 0x62 << 4;
		constexpr unsigned mustMatch = bit::bitmask_of_width(7) << 4;

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

		//make sure CAN peripherals have snychronized with the bus
		peripheralAwaitSynchronization(*CAN1);
		peripheralAwaitSynchronization(*CAN2);
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
		return SystemTimer::GetUptime().toMilliseconds();
	}

	//return -1 if we don't care about given message
	int txHandleCANMessage(uint32_t timestamp, int bus, CAN_ID_t id, const void* data, size_t length) {
		//valid bootloader messages have id in range 0x620-0x62F. CAN peripheral filters are set to match these values
		//but we may want to filter further messages we don't care about.
		constexpr std::uint32_t startID = 0x620;
		constexpr char care = 1, dont_care = -1;
		static constexpr std::array<char, 16> const return_codes{
			/*0*/ dont_care,
			/*1*/ dont_care,
			/*2*/ care, //Beacon
			/*3*/ care, //Data
			/*4*/ care, //DataAck
			/*5*/ care, //ExitReq
			/*6*/ dont_care,
			/*7*/ dont_care,
			/*8*/ dont_care,
			/*9*/ dont_care,
			/*A*/ care, //Handshake
			/*B*/ care, //HandshakeAck
			/*C*/ dont_care,
			/*D*/ dont_care,
			/*E*/ dont_care,
			/*F*/ care //CommunicationYield
		};

		return return_codes[id - startID];
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

	txReceiveCANMessage(bus_connected_to_CAN1, RxMessage.StdId, RxMessage.Data, RxMessage.DLC);
}

extern "C" void CAN2_RX0_IRQHandler(void)
{
	CanRxMsg RxMessage;

	CAN_Receive(CAN2, CAN_FIFO0, &RxMessage);

	txReceiveCANMessage(bus_connected_to_CAN2, RxMessage.StdId, RxMessage.Data, RxMessage.DLC);
}




/* END OF FILE */

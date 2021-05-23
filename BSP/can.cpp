#include "can.hpp"
#include "timer.hpp"

#include <bit>
#include <cstring>

#include <ufsel/bit_operations.hpp>


#include <library/assert.hpp>
#include <ufsel/units.hpp>
#include <CANdb/tx2/tx.h>
#include <CANdb/can_Bootloader.h>

namespace bsp::can {
	namespace {

		void peripheralRequestInitialization(CAN_TypeDef& can) {
			using namespace ufsel;

			//exit sleep mode and enter initialization
			std::uint32_t const copy = bit::clear(can.MCR, CAN_MCR_SLEEP);
			can.MCR = bit::set(copy, CAN_MCR_INRQ);
		}

		//wait for bxCAN to synchronize with the bus (leave initialization)
		void peripheralAwaitSynchronization(CAN_TypeDef& can) {
			ufsel::bit::wait_until_cleared(can.MSR, CAN_MSR_INAK);
		}

		void peripheralInit(CAN_TypeDef& can, unsigned prescaler) {
			using namespace ufsel;
			//await acknowledge that the peripheral entered initialization mode
			ufsel::bit::wait_until_set(can.MSR, CAN_MSR_INAK);

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

			//Enable the Fifo1 Message Pending Iterrupt for this peripheral
			ufsel::bit::set(std::ref(can.IER), CAN_IER_FMPIE0);

			//request to enter normal mode
			bit::clear(std::ref(can.MCR), CAN_MCR_INRQ);
		}
	}

	void Initialize() {
		using namespace ufsel;

		//enable peripheral clock to CAN1, CAN2
		bit::set(std::ref(RCC->APB1ENR), RCC_APB1ENR_CAN1EN, RCC_APB1ENR_CAN2EN);
		//wake up both peripherals and send them to initialization mode
		peripheralRequestInitialization(*CAN1);
		peripheralRequestInitialization(*CAN2);

		constexpr Frequency APB1_frequency = 12'000'000_Hz;
		constexpr int quanta_per_bit = 6;

		//Initialize peripherals
		peripheralInit(*CAN1, APB1_frequency / can1_frequency / quanta_per_bit);
		peripheralInit(*CAN2, APB1_frequency / can2_frequency / quanta_per_bit);

		//11 bits standard IDs. They share prefix 0x62_, the three bits are variable (range 0x620-0x627)
		constexpr unsigned sharedPrefix = 0x62 << 4;
		constexpr unsigned mustMatch = bit::bitmask_of_width(8) << 3;

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

	int txHandleCANMessage(uint32_t timestamp, int bus, CAN_ID_t id, const void* data, size_t length) {
		return 0; //all messages are filtered by hardware
	}

	int txSendCANMessage(int const bus, CAN_ID_t const id, const void* const data, size_t const length) {
		using namespace ufsel;
		assert(bus == bus_connected_to_CAN1 || bus == bus_connected_to_CAN2);

		CAN_TypeDef *const peripheral = bus == bus_connected_to_CAN1 ? CAN1 : CAN2;

		if (ufsel::bit::all_cleared(peripheral->TSR, CAN_TSR_TME))
			return -TX_SEND_BUFFER_OVERFLOW; //There is no empty mailbox

		std::array<std::uint32_t, 2> msg_data {0};
		std::memcpy(msg_data.data(), data, length);

		//Get the code of an empty mailbox.
		std::uint32_t const mailbox_index = ufsel::bit::sliceable_value{peripheral->TSR}[25_to, 24];
		std::uint32_t const corresponding_bit = ufsel::bit::bit(std::countr_zero(CAN_TSR_TME0) + mailbox_index);
		assert(ufsel::bit::all_set(peripheral->TSR, corresponding_bit)); //make sure that mailbox is really empty

		auto &mailbox = peripheral->sTxMailBox[mailbox_index];

		ufsel::bit::modify(std::ref(mailbox.TDTR), CAN_TDT0R_DLC, length); //write the message length
		if (length > 0) //and lower and higher word of data
			mailbox.TDLR = msg_data[0];
		if (length > 4)
			mailbox.TDHR = msg_data[1];


		//setup the identifier and request the transmission.
		mailbox.TIR = ufsel::bit::bitmask(id << std::countr_zero(CAN_TI0R_STID), CAN_TI0R_TXRQ);
		return 0;
	}
}

extern "C" void CAN1_RX0_IRQHandler(void)
{
	ufsel::bit::sliceable_value identifierReg{ CAN1->sFIFOMailBox[0].RIR };
	int const id = CAN1->sFIFOMailBox[0].RIR >> std::countr_zero(CAN_RI0R_STID);
	int const length = ufsel::bit::get(CAN1->sFIFOMailBox[0].RDTR, CAN_RDT0R_DLC);

	std::array<std::uint32_t, 2> data{ 0 };
	data[0] = CAN1->sFIFOMailBox[0].RDLR;
	data[1] = CAN1->sFIFOMailBox[0].RDHR;

	ufsel::bit::set(std::ref(CAN1->RF0R), CAN_RF0R_RFOM0);
	txReceiveCANMessage(bus_connected_to_CAN1, id, data.data(), length);
}

extern "C" void CAN2_RX0_IRQHandler(void)
{
	ufsel::bit::sliceable_value identifierReg {CAN2->sFIFOMailBox[0].RIR};
	int const id = CAN2->sFIFOMailBox[0].RIR >> std::countr_zero(CAN_RI0R_STID);
	int const length = ufsel::bit::get(CAN2->sFIFOMailBox[0].RDTR, CAN_RDT0R_DLC);

	std::array<std::uint32_t, 2> data {0};
	data[0] = CAN2->sFIFOMailBox[0].RDLR;
	data[1] = CAN2->sFIFOMailBox[0].RDHR;

	ufsel::bit::set(std::ref(CAN2->RF0R), CAN_RF0R_RFOM0);
	txReceiveCANMessage(bus_connected_to_CAN2, id, data.data(), length);
}




/* END OF FILE */

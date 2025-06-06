
#if !defined BOOT_STM32G4

#include "can.hpp"

#include <bit>
#include <cstring>

#include <ufsel/bit_operations.hpp>


#include <ufsel/assert.hpp>
#include <ufsel/time.hpp>
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
		void peripheralAwaitSynchronization(bus_info_t const& bus_info) {
			ufsel::bit::wait_until_cleared(bus_info.get_peripheral()->MSR, CAN_MSR_INAK);
		}

		void peripheralInit(bus_info_t const& bus_info) {

			CAN_TypeDef & can = *bus_info.get_peripheral();

			constexpr Frequency APB1_frequency = boot::SYSCLK;
			constexpr int quanta_per_bit = 16;

			unsigned const prescaler = APB1_frequency / find_bus_info_by_bus(bus_CAN1).bitrate / quanta_per_bit;

			using namespace ufsel;
			//await acknowledge that the peripheral entered initialization mode
			ufsel::bit::wait_until_set(can.MSR, CAN_MSR_INAK);

			constexpr bool ttcm = false,
				abom = true,
				awum = false,
				nart = false, //Maybe make this dynamic - start requreing ACK frames when transaction starts
				rflm = false,
				txfp = true;

			constexpr unsigned sjw = 1, bs1 = 13, bs2 = 2;
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

	void write_message_for_transmission(bus_info_t const &bus, MessageData const& msg) {
		CAN_TypeDef *const peripheral = bus.get_peripheral();

		using namespace ufsel;
		assert(has_empty_mailbox(peripheral));

		std::array<std::uint32_t, 2> msg_data {0};
		std::memcpy(msg_data.data(), &msg.data, msg.length);

		//Get the code of an empty mailbox.
		std::uint32_t const mailbox_index = ufsel::bit::sliceable_value{peripheral->TSR}[25_to, 24];
		std::uint32_t const corresponding_bit = ufsel::bit::bit(std::countr_zero(CAN_TSR_TME0) + mailbox_index);
		assert(ufsel::bit::all_set(peripheral->TSR, corresponding_bit)); //make sure that mailbox is really empty

		auto &mailbox = peripheral->sTxMailBox[mailbox_index];

		ufsel::bit::modify(std::ref(mailbox.TDTR), CAN_TDT0R_DLC, msg.length); //write the message length
		if (msg.length > 0) //and lower and higher word of data
			mailbox.TDLR = msg_data[0];
		if (msg.length > 4)
			mailbox.TDHR = msg_data[1];


		//setup the identifier and request the transmission.
		mailbox.TIR = ufsel::bit::bitmask(msg.id << std::countr_zero(CAN_TI0R_STID), CAN_TI0R_TXRQ);
	}

	void initialize() {
		using namespace ufsel;

		//enable peripheral clock to CAN1, CAN2
		bit::set(std::ref(RCC->APB1ENR), RCC_APB1ENR_CAN1EN, RCC_APB1ENR_CAN2EN);
		//wake up both peripherals and send them to initialization mode
#if CAN1_used
		peripheralRequestInitialization(*find_bus_info_by_bus(bus_CAN1).get_peripheral());
#endif
#if CAN2_used
		peripheralRequestInitialization(*find_bus_info_by_bus(bus_CAN2).get_peripheral());
#endif

		//Initialize peripherals
#if CAN1_used
		peripheralInit(find_bus_info_by_bus(bus_CAN1));
#endif
#if CAN2_used
		peripheralInit(find_bus_info_by_bus(bus_CAN2));
#endif

		assert(std::ranges::all_of(candb_received_messages,[](unsigned id) {
			// Make sure all received messages match the input filter (= they share the shared prefix)
			return (id & filter::sharedPrefix) == filter::sharedPrefix;
		}));

		//All filters must be accessed via CAN1, because
		//[ref manual f105 24.9.5] In connectivity line devices, the registers from offset 0x200 to 31C are present only in CAN1.
		bit::set(std::ref(CAN1->FMR), CAN_FMR_FINIT);

		//Configure filters in mask mode
		bit::modify(std::ref(CAN1->FM1R), bit::bitmask_of_width(2), 0);

		bit::modify(std::ref(CAN1->FS1R), bit::bitmask_of_width(2), 0b11); //make filters 32bits wide

		bit::modify(std::ref(CAN1->FFA1R), bit::bitmask_of_width(2), 0); //assign filters to FIFO 0

		//activate filters (they can still be modified, because FINIT flag overrides this)
		bit::modify(std::ref(CAN1->FA1R), bit::bitmask_of_width(2), 0b11);

		CAN1->sFilterRegister[0].FR1 = CAN1->sFilterRegister[1].FR1 = filter::sharedPrefix << (5 + 16);
		CAN1->sFilterRegister[0].FR2 = CAN1->sFilterRegister[1].FR2 = filter::mustMatch << (5 + 16);

		//Each CAN peripheral gets only one filter.
		bit::modify(std::ref(CAN1->FMR), bit::bitmask_of_width(6), 1, 8);
		bit::clear(std::ref(CAN1->FMR), CAN_FMR_FINIT);

		NVIC_EnableIRQ(CAN1_RX0_IRQn);
		NVIC_EnableIRQ(CAN2_RX0_IRQn);

		//make sure CAN peripherals have snychronized with the bus
#if CAN1_used
		peripheralAwaitSynchronization(find_bus_info_by_bus(bus_CAN1));
#endif
#if CAN2_used
		peripheralAwaitSynchronization(find_bus_info_by_bus(bus_CAN2));
#endif
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
	txReceiveCANMessage(bsp::can::find_bus_info_by_peripheral(CAN1_BASE).candb_bus, id, data.data(), length);
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
	txReceiveCANMessage(bsp::can::find_bus_info_by_peripheral(CAN2_BASE).candb_bus, id, data.data(), length);
}



#endif
/* END OF FILE */

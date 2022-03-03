
#if defined BOOT_STM32G4

#include "fdcan.hpp"
#include "timer.hpp"

#include <cstring>

#include <ufsel/bit_operations.hpp>
#include <ufsel/assert.hpp>

#include <can_Bootloader.h>
constexpr auto bus_connected_to_CAN1 = bus_CAN1;
constexpr auto bus_connected_to_CAN2 = bus_CAN2;


namespace bsp::can {
	namespace {

		struct bit_time_config {
			int nominal_prescaler, data_prescaler, sjw, bs1, bs2;

			[[nodiscard]]
			constexpr bool configuration_valid() const {
				return
					1 <= sjw && sjw <= 16 && sjw < bs2 &&
					1 <= bs2 && bs2 <= 16 &&
					1 <= bs1 && bs1 <= 32;
			}

			[[nodiscard]]
			constexpr int time_quanta_per_bit() const {
				// bit time = bs1 + sb2 + 1 (=sync seg)
				return 1 + bs1 + bs2;
			}

			[[nodiscard]]
			constexpr int kernel_clock_to_data_bit_time_ratio() const {
				return data_prescaler * (time_quanta_per_bit());
			}
			[[nodiscard]]
			constexpr int kernel_clock_to_nominal_bit_time_ratio() const {
				return nominal_prescaler * (time_quanta_per_bit());
			}
		};

		void peripheralRequestInitialization(FDCAN_GlobalTypeDef & can) {
			using namespace ufsel;

			// enter initialization
			bit::set(std::ref(can.CCCR), FDCAN_CCCR_INIT);
		}

		//wait for FDCAN to synchronize with the bus (leave initialization)
		void peripheralAwaitSynchronization(FDCAN_GlobalTypeDef & can) {
			//TODO does this work with FDCAN as well? I only know this process with bxCAN
			ufsel::bit::wait_until_cleared(can.CCCR, FDCAN_CCCR_INIT);
		}

		void peripheralInit(FDCAN_GlobalTypeDef * const can, bit_time_config const bit_time_config) {
			using namespace ufsel;
			//await acknowledge that the peripheral entered initialization mode
			bit::wait_until_set(can->CCCR, FDCAN_CCCR_INIT);

			bit::set(std::ref(can->DBTP),
					// do not enable transceiver delay compensation
					(bit_time_config.data_prescaler - 1) << FDCAN_DBTP_DBRP_Pos, // configure data prescaler
					(bit_time_config.bs1 - 1) << FDCAN_DBTP_DTSEG1_Pos, // configure segment 1&2 length
					(bit_time_config.bs2 - 1) << FDCAN_DBTP_DTSEG2_Pos,
					(bit_time_config.sjw - 1) << FDCAN_DBTP_DSJW_Pos // configure synchronization jump width
			);

			// Keep the message RAM watchdog disabled for now

			bit::set(std::ref(can->CCCR),
					// keep ISO CANFD operation
					FDCAN_CCCR_TXP, // insert a delay of 2 bit times after successful frame TX
					// edge filtering disabled
					// protocol exception handling disabled
					// disable bit rate switching
					// disable FD operation
					// disabled test mode
					// disabled bus monitoring mode
					// no clock stop request, no power down
					// no restricted operation (for now?)
					FDCAN_CCCR_CCE // allow write access to config registers
			);

			bit::set(std::ref(can->NBTP),
					// do not enable transceiver delay compensation
					(bit_time_config.nominal_prescaler - 1) << FDCAN_NBTP_NBRP_Pos, // the nominal baudrate prescaler
					(bit_time_config.bs1 - 1) << FDCAN_NBTP_NTSEG1_Pos, // configure segment 1&2 length
					(bit_time_config.bs2 - 1) << FDCAN_NBTP_NTSEG2_Pos,
					(bit_time_config.sjw - 1) << FDCAN_NBTP_NSJW_Pos // configure synchronization jump width
			);

			bit::set(std::ref(can->TSCC),
					// default prescaler - count in bit times (1 us for CAN2, 2 us for CAN1)
					0b01 << FDCAN_TSCC_TSS_Pos // Use internal timestamp counter
			);

			// Dont care about timeout

			// Nothing to configure in protocol status register, error counter register

			// Ignore transmitter delay compensation

			// Enable the "not empty" interrupt of RX FIFO 0
			bit::set(std::ref(can->IE), FDCAN_IE_RF0NE);

			bit::set(std::ref(can->ILS),
					// Generate RX FIFO 0 interrupts on interrupt line 0 (default)
					0
			);

			// Enable CAN interrupt line 0 (for RX FIFO interrupts)
			bit::set(std::ref(can->ILE), FDCAN_ILE_EINT0);

			// Dont use XIDAM register (default value of all zeros => masking is not active)

			// Dont use high priority messages (for now?) //TODO consider using them for torque requirements

			// Nothing to configure in RX FIFO status and ack registers

			bit::set(std::ref(can->TXBC),
					// Transmit using FIFO mode (order TX messages by insertion time, not by priority)
					0
			);

			// Nothing to configure in TX FIFO status reg, cancellation request reg, buffer add request etc
		}

		void filtersInit(FDCAN_GlobalTypeDef * const can) {
			using namespace ufsel;
			MessageRAM_TypeDef * const ram = get_message_ram_for_periph(can);

			assert(std::ranges::all_of(candb_received_messages, [](unsigned id) {
				// Make sure all received messages match the input filter (= they share the shared prefix)
				return (id & filter::sharedPrefix) == filter::sharedPrefix;
			}));

			bit::set(std::ref(can->RXGFC),
					0 << FDCAN_RXGFC_LSE_Pos, // only one standard filter
					1 << FDCAN_RXGFC_LSS_Pos,
					// RXFIFO0 operates in blocking mode (default)
					0b10 << FDCAN_RXGFC_ANFS_Pos,// reject non-matching standard frames
					0b10 << FDCAN_RXGFC_ANFE_Pos,// reject non-matching extended frames
					FDCAN_RXGFC_RRFS, // reject standard remote frames
					FDCAN_RXGFC_RRFE // reject extended remote frames
			);

			// Initialize standard filters
			bit::sliceable_with_deffered_writeback filter(ram->std_filter[0].S0);
			filter[bit::slice::for_mask(STD_Filter::S0_SFT_Msk)] = 0b10; // Classic filter with mask
			filter[bit::slice::for_mask(STD_Filter::S0_SFEC_Msk)] = 0b001; // Store to RX FIFO 0 without priority
			filter[bit::slice::for_mask(STD_Filter::S0_SFID1_Msk)] = filter::sharedPrefix; // Prefix shared by all bootloader messages
			filter[bit::slice::for_mask(STD_Filter::S0_SFID2_Msk)] = filter::mustMatch; // "Must match mask"

			// Ignore extended filters (bootloader only uses standard IDs)
		}

		constexpr bool verify_bit_time_config(bit_time_config const config, bus_baudrate const baudrate) {
			return
				config.time_quanta_per_bit() == time_quanta_per_bit &&
				config.kernel_clock_to_nominal_bit_time_ratio() * baudrate.nominal == kernel_clock_frequency &&
				config.kernel_clock_to_data_bit_time_ratio() * baudrate.data == kernel_clock_frequency;
		}
	}

	void Initialize() {
		using namespace ufsel;

#if 0
		// No clock input prescaling is used
		constexpr int periph_clock_prescaler = bsp::clock::bus_speed::APB1 / can_clock_frequency;
		constexpr int prescaler_reg_value = periph_clock_prescaler == 1 ? 0 : periph_clock_prescaler / 2;
		// Prescale CAN kernel clock frequency
		bit::modify(std::ref(FDCAN_CONFIG->CKDIV), FDCAN_CKDIV_PDIV_Msk, prescaler_reg_value);
#endif
		//wake up both peripherals and send them to initialization mode
		peripheralRequestInitialization(*FDCAN1);
		peripheralRequestInitialization(*FDCAN2);

		//Initialize peripherals
		constexpr bit_time_config bit_time_config1{
			.nominal_prescaler = 12, .data_prescaler = 12, .sjw = 1, .bs1 = 3, .bs2 = 2
		};
		constexpr bit_time_config bit_time_config2{
			.nominal_prescaler = 6, .data_prescaler = 6, .sjw = 1, .bs1 = 3, .bs2 = 2
		};
		// Sanity checks that the configuration is valid:
		static_assert(verify_bit_time_config(bit_time_config1, can1_baudrate));
		static_assert(verify_bit_time_config(bit_time_config2, can2_baudrate));

		peripheralInit(FDCAN1, bit_time_config1);
		peripheralInit(FDCAN2, bit_time_config2);

		NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
		NVIC_EnableIRQ(FDCAN2_IT0_IRQn);

		//Filters, intialize the same set for both peripherals such that it does
		// not matter what bus the transmitter uses

		filtersInit(FDCAN1);
		filtersInit(FDCAN2);

		// Exit the initialization mode on both peripherals
		bit::clear(std::ref(FDCAN1->CCCR), FDCAN_CCCR_CCE, FDCAN_CCCR_INIT);
		bit::clear(std::ref(FDCAN2->CCCR), FDCAN_CCCR_CCE, FDCAN_CCCR_INIT);

		//make sure CAN peripherals have snychronized with the bus
		peripheralAwaitSynchronization(*FDCAN1);
		peripheralAwaitSynchronization(*FDCAN2);
	}

	//Read message data (and then release it) from the FIFO0 of given peripheral.
	MessageData extractPendingMessage(FDCAN_GlobalTypeDef * const can) {
		MessageRAM_TypeDef * const ram = get_message_ram_for_periph(can);
		using namespace ufsel;
		// Get the read pointer into the reception queue
		int const get_index = bit::get(can->RXF0S,FDCAN_RXF0S_F0GI) >> FDCAN_RXF0S_F0GI_Pos;

		auto const& rx_buffer = ram->rx_fifo0[get_index];
		bit::sliceable_value const R0{rx_buffer.R0};
		bool const is_extended = R0[bit::slice::for_mask(RX_FIFO::R0_XTD_Msk)];
		// ignore ESI and remote frames (they are rejected)
		std::uint32_t const message_id = [&]() -> CAN_ID_t {
			if (is_extended)
				return EXT_ID(R0[bit::slice::for_mask(RX_FIFO::R0_ID_Msk_EXT)]);
			else
				return STD_ID(R0[bit::slice::for_mask(RX_FIFO::R0_ID_Msk_STD)]);
		}();

		bit::sliceable_value const R1{rx_buffer.R1};
		// Ignore matched filter index
		// Ignore frame format, bitrate switching
		std::uint32_t const length = DLC_to_length(R1[bit::slice::for_mask(RX_FIFO::R1_DLC_Msk)]);
		// ignore RX timestamp
		std::uint32_t const word_count = (length + sizeof(std::uint32_t) - 1) / sizeof(std::uint32_t);

		MessageData result {.id = message_id, .length = length};
		// Read message data out from the peripheral
		std::copy_n(rx_buffer.data, word_count, result.data.begin());

		// Acknowledge data extraction, advance the read pointer
		can->RXF0A = get_index;
		return result;
	}

	void insertMessageForTransmission(FDCAN_GlobalTypeDef * const can, MessageData const& msg) {
		MessageRAM_TypeDef * const ram = get_message_ram_for_periph(can);
		using namespace ufsel;
		assert(hasEmptyMailbox(can));
		// Get the write pointer into the reception queue
		int const write_index = bit::get(can->TXFQS,FDCAN_TXFQS_TFQPI) >> FDCAN_TXFQS_TFQPI_Pos;

		auto & tx_buffer = ram->tx_buffers[write_index];
		bool const is_extended = IS_EXT_ID(msg.id);
		{
			bit::sliceable_with_deffered_writeback T0(tx_buffer.T0);
			T0[bit::slice::for_mask(is_extended ? TX_Buffer::T0_ID_Msk_EXT : TX_Buffer::T0_ID_Msk_STD)] = msg.id;
			T0[bit::slice::for_mask(TX_Buffer::T0_XTD_Msk)] = is_extended;
		}
		auto const DLC = length_to_DLC(msg.length);
		assert(DLC.has_value() && "You attempted to transmit a message of unsupported length!");
		{
			bit::sliceable_with_deffered_writeback T1(tx_buffer.T1);
			T1[bit::slice::for_mask(TX_Buffer::T1_MM_Msk)] = 0; // ignore message marker (keep zero)
			T1[bit::slice::for_mask(TX_Buffer::T1_EFC_Msk)] = 0; // do not store TX event
			T1[bit::slice::for_mask(TX_Buffer::T1_FDF_Msk)] = 0; // normal (non-FD) frame
			T1[bit::slice::for_mask(TX_Buffer::T1_BRS_Msk)] = 0; // no bit rate switching
			T1[bit::slice::for_mask(TX_Buffer::T1_DLC_Msk)] = DLC.value();
		}
		int const word_count = (msg.length + sizeof(std::uint32_t) - 1) / sizeof(std::uint32_t);
		// Copy the message data into Message RAM
		std::copy_n(msg.data.begin(), word_count, tx_buffer.data);

		// Add request for this TX buffer element
		can->TXBAR = bit::bit(write_index);
	}
}


/* Implementation of function required by tx library.
   These functions must use C linkage, because they are used by the code generated from CANdb (which is pure C). */


extern "C" {
	uint32_t txGetTimeMillis() {
		return SystemTimer::GetUptime().toMilliseconds();
	}

	int txHandleCANMessage(uint32_t timestamp, int bus, CAN_ID_t id, const void* data, size_t length) {
		return 0; //all messages are filtered by hardware
	}

	int txSendCANMessage(int const bus, CAN_ID_t const id, const void* const data, size_t const length) {
		if (bus == bus_BOTH) {
			int const can1 = txSendCANMessage(bus_CAN1, id, data, length);
			int const can2 = txSendCANMessage(bus_CAN2, id, data, length);
			return can1 && can2;
		}

		using namespace ufsel;
		assert(bus == bus_connected_to_CAN1 || bus == bus_connected_to_CAN2);

		FDCAN_GlobalTypeDef * const peripheral = bus == bus_connected_to_CAN1 ? FDCAN1 : FDCAN2;

		if (!bsp::can::hasEmptyMailbox(peripheral))
			return -TX_SEND_BUFFER_OVERFLOW; //There is no empty mailbox

		bsp::can::MessageData message {.id = id, .length = length};
		std::memcpy(message.data.data(), data, length);

		bsp::can::insertMessageForTransmission(peripheral, message);
		return 0;
	}
}

extern "C" void FDCAN1_IT0_IRQHandler(void)
{
	using namespace bsp::can;
	assert(ufsel::bit::all_set(FDCAN1->IR, FDCAN_IR_RF0N));

	MessageData const msg = extractPendingMessage(FDCAN1);
	ufsel::bit::set(std::ref(FDCAN1->IR), FDCAN_IR_RF0N); // clear the interrupt flag
	txReceiveCANMessage(bus_connected_to_CAN1, msg.id, msg.data.data(), msg.length);
}

extern "C" void FDCAN2_IT0_IRQHandler(void)
{
	using namespace bsp::can;
	assert(ufsel::bit::all_set(FDCAN2->IR, FDCAN_IR_RF0N));

	MessageData const msg = extractPendingMessage(FDCAN2);
	ufsel::bit::set(std::ref(FDCAN2->IR), FDCAN_IR_RF0N); // clear the interrupt flag
	txReceiveCANMessage(bus_connected_to_CAN2, msg.id, msg.data.data(), msg.length);
}



#endif
/* END OF FILE */

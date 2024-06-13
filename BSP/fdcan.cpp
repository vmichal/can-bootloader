
#if defined BOOT_STM32G4

#include "fdcan.hpp"
#include <ufsel/time.hpp>

#include <Drivers/stm32g4xx.h>

#include <cstring>

#include <ufsel/bit_operations.hpp>
#include <ufsel/assert.hpp>
#include <ufsel/itertools.hpp>

#include <can_Bootloader.h>

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

		void request_peripheral_initialization(FDCAN_GlobalTypeDef * const can) {
			using namespace ufsel;

			// enter initialization
			bit::set(std::ref(can->CCCR), FDCAN_CCCR_INIT, FDCAN_CCCR_CCE);
		}

		//wait for FDCAN to synchronize with the bus (leave initialization)
		void await_peripheral_synchronization(FDCAN_GlobalTypeDef * const can) {
			ufsel::bit::wait_until_cleared(can->CCCR, FDCAN_CCCR_INIT);
		}

		void initialize_peripheral(FDCAN_GlobalTypeDef * const can, bit_time_config const bit_time_config) {
			using namespace ufsel;
			//await acknowledge that the peripheral entered initialization mode
			bit::wait_until_set(can->CCCR, FDCAN_CCCR_INIT);

			can->DBTP = bit::bitmask(
					// do not enable transceiver delay compensation
					(bit_time_config.data_prescaler - 1) << FDCAN_DBTP_DBRP_Pos, // configure data prescaler
					(bit_time_config.bs1 - 1) << FDCAN_DBTP_DTSEG1_Pos, // configure segment 1&2 length
					(bit_time_config.bs2 - 1) << FDCAN_DBTP_DTSEG2_Pos,
					(bit_time_config.sjw - 1) << FDCAN_DBTP_DSJW_Pos // configure synchronization jump width
			);

			// Keep the message RAM watchdog disabled for now

			bit::set(std::ref(can->CCCR),
					// keep ISO CANFD operation
					FDCAN_CCCR_TXP, // insert a delay of 2 bit times after successful frame TX // TODO make this a customization point in the future
					// edge filtering disabled
					// protocol exception handling disabled
					FDCAN_CCCR_BRSE, // permit bit rate switching
					FDCAN_CCCR_FDOE // permit FD operation
					// disabled test mode
					// disabled bus monitoring mode
					// no clock stop request, no power down
					// no restricted operation (for now?)
			);

			can->NBTP = bit::bitmask(
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

			// Enable the "not empty" interrupt of RX FIFO 0 and Bus Off status change
			bit::set(std::ref(can->IE), FDCAN_IE_BOE, FDCAN_IE_RF0NE);

			bit::set(std::ref(can->ILS),
					// Generate RX FIFO 0 interrupts on interrupt line 0 (default)
					// Generate Protocol errors (such as bus off and warning ) on interrupt line 1
					FDCAN_ILS_PERR
			);

			// Enable CAN interrupt line 0 (for RX FIFO interrupts)
			// Enable CAN interrupt line 1 (for Bus Off)
			bit::set(std::ref(can->ILE), FDCAN_ILE_EINT0, FDCAN_ILE_EINT1);

			// Dont use XIDAM register (default value of all zeros => masking is not active)

			// Dont use high priority messages (for now?) //TODO consider using them for torque requirements

			// Nothing to configure in RX FIFO status and ack registers

			bit::set(std::ref(can->TXBC),
					// Transmit using FIFO mode (order TX messages by insertion time, not by priority)
					0
			);

			// Nothing to configure in TX FIFO status reg, cancellation request reg, buffer add request etc
		}

		void init_filters(FDCAN_GlobalTypeDef * const can) {
			MessageRAM_TypeDef * const ram = get_message_ram_for_periph(can);
			constexpr auto has_std_id = [](auto const id) { return IS_STD_ID(id); };
			constexpr auto has_ext_id = [](auto const id) { return IS_EXT_ID(id); };
			using namespace ufsel;
			// Filter initialization
			std::size_t const rx_ext_id_count = std::ranges::count_if(candb_received_messages, has_ext_id);
			std::size_t const rx_std_id_count = std::ranges::count_if(candb_received_messages, has_std_id);
			assert(rx_ext_id_count + rx_std_id_count == std::size(candb_received_messages));

			constexpr int ids_per_filter = 2;
			// The number of std and ext filters to be activated
			std::size_t const std_filter_count = (rx_std_id_count + ids_per_filter - 1) / ids_per_filter;
			std::size_t const ext_filter_count = (rx_ext_id_count + ids_per_filter - 1) / ids_per_filter;


			constexpr int max_ext_filters = 8, max_std_filters = 28;
			static_assert(std::extent_v<decltype(candb_received_messages)> < (max_ext_filters + max_std_filters) * ids_per_filter,
					"Identifier list filtering method requires more filters than the hardware exposes. Use mask/range filtering!");
			assert(ext_filter_count <= max_ext_filters);
			assert(std_filter_count <= max_std_filters);

			bit::set(std::ref(can->RXGFC),
					ext_filter_count << FDCAN_RXGFC_LSE_Pos, // configure the length of extended filter list
					std_filter_count << FDCAN_RXGFC_LSS_Pos, // configure the length of standard filter list
					// RXFIFO0 operates in blocking mode (default)
					0b10 << FDCAN_RXGFC_ANFS_Pos,// reject non-matching standard frames
					0b10 << FDCAN_RXGFC_ANFE_Pos,// reject non-matching extended frames
					FDCAN_RXGFC_RRFS, // reject standard remote frames
					FDCAN_RXGFC_RRFE // reject extended remote frames
			);
			// Initialize standard filters
			bit::sliceable_with_deffered_writeback filter(ram->std_filter[0].S0);
			filter[bit::slice::for_mask(message_RAM::STD_Filter::S0_SFT_Msk)] = 0b10; // Classic filter with mask
			filter[bit::slice::for_mask(message_RAM::STD_Filter::S0_SFEC_Msk)] = 0b001; // Store to RX FIFO 0 without priority
			filter[bit::slice::for_mask(message_RAM::STD_Filter::S0_SFID1_Msk)] = filter::sharedPrefix; // Prefix shared by all bootloader messages
			filter[bit::slice::for_mask(message_RAM::STD_Filter::S0_SFID2_Msk)] = filter::mustMatch; // "Must match mask"

			// Ignore extended filters (bootloader only uses standard IDs)
		}

		template<std::size_t N, bit_time_config config>
		struct bit_time_config_ok_helper {
			constexpr static bus_info_t bus = bus_info[N];
			// Using 16 time quanta is suggested (among others) here http://www.bittiming.can-wiki.info/ for normal CAN. We lack experience for CANFD.
			static_assert(config.time_quanta_per_bit() == 16, "Ideally use 16 Tq per bit to ensure the best sampling point position (87.5 %). Check the CAN bus number in this struct's template argument.");
			static_assert(config.kernel_clock_to_nominal_bit_time_ratio() * bus.bitrate_nominal == kernel_clock_frequency, "The specified prescaler does not yield the correct NOMINAL BITRATE. Check the CAN bus number in this struct's template argument.");
			static_assert(config.kernel_clock_to_data_bit_time_ratio() * bus.bitrate_data == kernel_clock_frequency, "The specified prescaler does not yield the correct DATA BITRATE. Check the CAN bus number in this struct's template argument.");
			static constexpr bool value = N == 0 ? true : bit_time_config_ok_helper<N - 1, config>::value;

		};

		template<bit_time_config config>
		struct bit_time_config_ok_helper<0, config> : std::true_type {};

		template<bit_time_config config>
		struct bit_time_config_ok : bit_time_config_ok_helper<std::size(bus_info) - 1, config> {};
	}

	void initialize() {
		using namespace ufsel;

		bit::set(std::ref(RCC->APB1ENR1), RCC_APB1ENR1_FDCANEN); // enable FDCAN clock

		bit::set(std::ref(RCC->CCIPR),
				0b10 << RCC_CCIPR_FDCANSEL_Pos // FDCAN clocked from APB
		);

#if 0
		// No clock input prescaling is used
		constexpr int periph_clock_prescaler = bsp::clock::bus_speed::APB1 / can_clock_frequency;
		constexpr int prescaler_reg_value = periph_clock_prescaler == 1 ? 0 : periph_clock_prescaler / 2;
		// Prescale CAN kernel clock frequency
		bit::modify(std::ref(FDCAN_CONFIG->CKDIV), FDCAN_CKDIV_PDIV_Msk, prescaler_reg_value);
#endif
		//wake up peripherals and send them to initialization mode
		for (auto bus : bus_info)
			request_peripheral_initialization(bus.get_peripheral());

		//Initialize peripherals. 16 time quanta per bit are really heavilly recommended, hence it will be used for all periphs
		// Using 16 time quanta is suggested (among others) here http://www.bittiming.can-wiki.info/ for normal CAN. We lack experience for CANFD.
		constexpr bit_time_config bit_time_config{
				.nominal_prescaler = 4, .data_prescaler = 4, .sjw = 1, .bs1 = 13, .bs2 = 2
		};
		// Sanity checks that the configuration is valid:
		static_assert(bit_time_config_ok<bit_time_config>::value);

		for (auto bus : bus_info)
			initialize_peripheral(bus.get_peripheral(), bit_time_config);

		// Enable interrupts for all peripherals. If some peripheral isn't used,
		// it will not be initialized and hence will not fire any IRQ.
		NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
		NVIC_EnableIRQ(FDCAN1_IT1_IRQn);
		NVIC_EnableIRQ(FDCAN2_IT0_IRQn);
		NVIC_EnableIRQ(FDCAN2_IT1_IRQn);
		NVIC_EnableIRQ(FDCAN3_IT0_IRQn);
		NVIC_EnableIRQ(FDCAN3_IT1_IRQn);
		NVIC_SetPriority(FDCAN1_IT0_IRQn, 10); // Low priority
		NVIC_SetPriority(FDCAN1_IT1_IRQn, 10); // Low priority
		NVIC_SetPriority(FDCAN2_IT0_IRQn, 10); // Low priority
		NVIC_SetPriority(FDCAN2_IT1_IRQn, 10); // Low priority
		NVIC_SetPriority(FDCAN3_IT0_IRQn, 10); // Low priority
		NVIC_SetPriority(FDCAN3_IT1_IRQn, 10); // Low priority

		//Filters, intialize the same set for both peripherals such that it does
		// not matter what bus the transmitter uses

		for (auto bus : bus_info)
			init_filters(bus.get_peripheral());

		// Exit the initialization mode on both peripherals
		for (auto bus : bus_info)
			bit::clear(std::ref(bus.get_peripheral()->CCCR), FDCAN_CCCR_CCE, FDCAN_CCCR_INIT);

		//make sure CAN peripherals have snychronized with the bus
		for (auto bus : bus_info)
			await_peripheral_synchronization(bus.get_peripheral());
	}

	//Read message data (and then release it) from the FIFO0 of given peripheral.
	MessageData read_message(FDCAN_GlobalTypeDef * const can) {
		MessageRAM_TypeDef * const ram = get_message_ram_for_periph(can);
		using namespace ufsel;
		// Get the read pointer into the reception queue
		int const get_index = bit::get(can->RXF0S,FDCAN_RXF0S_F0GI) >> FDCAN_RXF0S_F0GI_Pos;

		auto const& rx_buffer = ram->rx_fifo0[get_index];
		bit::sliceable_value const R0{rx_buffer.R0};
		bool const is_extended = R0[bit::slice::for_mask(message_RAM::RX_FIFO::R0_XTD_Msk)];
		// ignore ESI and remote frames (they are rejected)
		std::uint32_t const message_id = [&]() -> CAN_ID_t {
			if (is_extended)
				return EXT_ID(R0[bit::slice::for_mask(message_RAM::RX_FIFO::R0_ID_Msk_EXT)]);
			else
				return STD_ID(R0[bit::slice::for_mask(message_RAM::RX_FIFO::R0_ID_Msk_STD)]);
		}();

		bit::sliceable_value const R1{rx_buffer.R1};
		// Ignore matched filter index
		// Ignore frame format, bitrate switching
		std::uint32_t const length = DLC_to_length(R1[bit::slice::for_mask(message_RAM::RX_FIFO::R1_DLC_Msk)]);
		// ignore RX timestamp
		int const word_count = (length + sizeof(std::uint32_t) - 1) / sizeof(std::uint32_t);

		MessageData result {.id = message_id, .length = length};

		// Copy the message data from Message RAM
		// Cannot use std::copy here since it is strictly necessary to use word accesses
		for (int word = 0; word < word_count; ++word)
			result.data[word] = rx_buffer.data[word];

		// Acknowledge data extraction, advance the read pointer
		can->RXF0A = get_index;
		return result;
	}

	void write_message_for_transmission(bus_info_t const &bus, MessageData const& msg) {
		FDCAN_GlobalTypeDef * const can = bus.get_peripheral();
		MessageRAM_TypeDef * const ram = get_message_ram_for_periph(can);
		using namespace ufsel;
		assert(has_empty_mailbox(can));
		// Get the write pointer into the reception queue
		int const write_index = bit::get(can->TXFQS,FDCAN_TXFQS_TFQPI) >> FDCAN_TXFQS_TFQPI_Pos;

		auto & tx_buffer = ram->tx_buffers[write_index];
		bool const is_extended = IS_EXT_ID(msg.id);
		{
			bit::sliceable_value T0(0);
			T0[bit::slice::for_mask(is_extended ? message_RAM::TX_Buffer::T0_ID_Msk_EXT : message_RAM::TX_Buffer::T0_ID_Msk_STD)] = msg.id;
			T0[bit::slice::for_mask(message_RAM::TX_Buffer::T0_XTD_Msk)] = is_extended;
			tx_buffer.T0 = T0.value();
		}
		auto const DLC = length_to_DLC(msg.length);
		assert(DLC.has_value() && "You attempted to transmit a message of unsupported length!");
		{
			bit::sliceable_value T1(0);
			// TODO here we have to permit FD frames, bitrate switching etc.
			T1[bit::slice::for_mask(message_RAM::TX_Buffer::T1_MM_Msk)] = 0; // ignore message marker (keep zero)
			T1[bit::slice::for_mask(message_RAM::TX_Buffer::T1_EFC_Msk)] = 0; // do not store TX event // TODO support this at least for Ocarina global timing synchronization
			T1[bit::slice::for_mask(message_RAM::TX_Buffer::T1_FDF_Msk)] = bus.fd_frame; // normal or FD frame
			T1[bit::slice::for_mask(message_RAM::TX_Buffer::T1_BRS_Msk)] = bus.bitrate_switching; // bit rate switching
			T1[bit::slice::for_mask(message_RAM::TX_Buffer::T1_DLC_Msk)] = DLC.value();
			tx_buffer.T1 = T1.value();
		}
		int const word_count = (msg.length + sizeof(std::uint32_t) - 1) / sizeof(std::uint32_t);
		// Copy the message data into Message RAM
		// Cannot use std::copy here since it is strictly necessary to use word accesses
		// and std::copy had the tendency to fall back to memcpy which resulted in some byte accesses.
		for (int word = 0; word < word_count; ++word)
			tx_buffer.data[word] = msg.data[word];

		// Add request for this TX buffer element
		can->TXBAR = bit::bit(write_index);
	}

	void handle_interrupt(bus_info_t const& bus_info) {
		FDCAN_GlobalTypeDef * const peripheral = bus_info.get_peripheral();

		assert(ufsel::bit::all_set(peripheral->IR, FDCAN_IR_RF0N));

		MessageData const msg = read_message(peripheral);
		ufsel::bit::set(std::ref(peripheral->IR), FDCAN_IR_RF0N); // clear the interrupt flag
		txReceiveCANMessage(bus_info.candb_bus, msg.id, msg.data.data(), msg.length);
	}

	void handle_bus_off_warning(bus_info_t const& bus_info) {
		using namespace ufsel;
		FDCAN_GlobalTypeDef * const peripheral = bus_info.get_peripheral();


		if (bit::all_set(peripheral->IR, FDCAN_IR_BO)){
			bit::clear(std::ref(peripheral->CCCR), FDCAN_CCCR_INIT);
		}
		else {
			assert_unreachable();
		}
		// TODO document what this assignment is
		peripheral->IR = 0xff'ff'ff'00;
	}
}

extern "C" {
uint32_t txGetTimeMillis() {
	return systemStartupTime.TimeElapsed().toMilliseconds();
}

int txHandleCANMessage(uint32_t timestamp, int bus, CAN_ID_t id, const void* data, size_t length) {
	return 0; //all messages are filtered by hardware
}

int txSendCANMessage(int const bus, CAN_ID_t const id, const void* const data, size_t const length) {
	if (bus == bus_ALL) {
		int const can1 = txSendCANMessage(bus_CAN1, id, data, length);
		int const can2 = txSendCANMessage(bus_CAN2, id, data, length);
		return can1 && can2;
	}

	using namespace ufsel;
	auto& bus_info = bsp::can::find_bus_info_by_bus((candb_bus_t)bus);

	if (!bsp::can::has_empty_mailbox(bus_info.get_peripheral()))
		return -TX_SEND_BUFFER_OVERFLOW; //There is no empty mailbox

	bsp::can::MessageData message {.id = id, .length = length};
	std::memcpy(message.data.data(), data, length);

	bsp::can::write_message_for_transmission(bus_info, message);
	return 0;
}
}

extern "C" void FDCAN1_IT0_IRQHandler(void) {
	bsp::can::handle_interrupt(bsp::can::find_bus_info_by_peripheral(FDCAN1_BASE));
}

extern "C" void FDCAN2_IT0_IRQHandler(void) {
	bsp::can::handle_interrupt(bsp::can::find_bus_info_by_peripheral(FDCAN2_BASE));
}

extern "C" void FDCAN3_IT0_IRQHandler(void) {
	bsp::can::handle_interrupt(bsp::can::find_bus_info_by_peripheral(FDCAN3_BASE));
}

extern "C" void FDCAN1_IT1_IRQHandler(void) {
	bsp::can::handle_bus_off_warning(bsp::can::find_bus_info_by_peripheral(FDCAN1_BASE));
}

extern "C" void FDCAN2_IT1_IRQHandler(void) {
	bsp::can::handle_bus_off_warning(bsp::can::find_bus_info_by_peripheral(FDCAN2_BASE));
}

extern "C" void FDCAN3_IT1_IRQHandler(void) {
	bsp::can::handle_bus_off_warning(bsp::can::find_bus_info_by_peripheral(FDCAN3_BASE));
}

#endif
/* END OF FILE */

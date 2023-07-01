#pragma once

#if defined BOOT_STM32G4

#include <ufsel/units.hpp>
#include <ufsel/assert.hpp>
#include <ufsel/bit_operations.hpp>
#include <array>
#include <optional>
#include <Bootloader/options.hpp>


namespace bsp::can {

	// Data for CAN filter configuration
	// 11 bits standard IDs. They share prefix 0x62_, the three bits are variable (range 0x620-0x627)
	namespace filter {
		constexpr unsigned sharedPrefix = 0x62 << 4;
		constexpr unsigned mustMatch = ufsel::bit::bitmask_of_width(8) << 3;
	}

	constexpr std::size_t max_canfd_data_length = 64;

	constexpr std::array data_lengths {
		0,1,2,3,
		4,5,6,7,
		8,12,16,20,
		24,32,48,64};
	static_assert(std::size(data_lengths) == 16);
	static_assert(data_lengths.back() == max_canfd_data_length);

	constexpr int DLC_to_length(int const DLC) {
		return data_lengths[DLC];
	}

	constexpr std::optional<int> length_to_DLC(int const length) {
		auto const iter = std::ranges::find(data_lengths, length);
		if (iter == data_lengths.end())
			return std::nullopt; // No DLC matches this data length
		return std::distance(data_lengths.begin(), iter);
	}

	struct bus_baudrate {
		Frequency nominal, data;
	};

	constexpr auto kernel_clock_frequency = boot::SYSCLK;
	constexpr bus_baudrate can1_baudrate{.nominal = GENERATE_CAN_FREQ(CAN1_FREQ), .data = GENERATE_CAN_FREQ(CAN1_FREQ)};
	constexpr bus_baudrate can2_baudrate{.nominal = 1'000_kHz, .data = 1'000_kHz};
	constexpr int time_quanta_per_bit = 6;

	constexpr std::size_t message_ram_word_count = 212;
	constexpr InformationSize message_ram_size = InformationSize::fromBytes(message_ram_word_count * 4);
	static_assert(message_ram_size.toBytes() == 0x350);

	namespace RX_FIFO {
		struct element {
			std::uint32_t R0;
			std::uint32_t R1;
			// Message data are stored such that
			// data[i] = data_byte[4*i] | data_byte[4*i + 1] << 8 | data_byte[4*i + 2] << 16 | data_byte[4*i + 3] << 24
			// i.e. the LSB is the LSB of stored word
			std::uint32_t data[max_canfd_data_length / sizeof(std::uint32_t)];
		};

		///////////////////////////////////////////////////////////////
		// Definition of bitmasks for interaction with RX FIFO elements
		///////////////////////////////////////////////////////////////

		constexpr std::uint32_t R0_ESI_Msk = ufsel::bit::bit(31);
		constexpr std::uint32_t R0_XTD_Msk = ufsel::bit::bit(30);
		constexpr std::uint32_t R0_RTR_Msk = ufsel::bit::bit(29);
		constexpr std::uint32_t R0_ID_Msk_EXT = ufsel::bit::bitmask_between(28, 0);
		constexpr std::uint32_t R0_ID_Msk_STD = ufsel::bit::bitmask_between(28, 18);

		constexpr std::uint32_t R1_ANMF_Msk = ufsel::bit::bit(31);
		constexpr std::uint32_t R1_FIDX_Msk = ufsel::bit::bitmask_between(30, 24);
		// R1 bits 23:22 reserved
		constexpr std::uint32_t R1_FDF_Msk = ufsel::bit::bit(21);
		constexpr std::uint32_t R1_BRS_Msk = ufsel::bit::bit(20);
		constexpr std::uint32_t R1_DLC_Msk = ufsel::bit::bitmask_between(19, 16);
		constexpr std::uint32_t R1_RXTS_Msk = ufsel::bit::bitmask_between(15, 0);
	}

	namespace TX_Buffer {
		struct element {
			// Bits T0_ESI, T1_FDF and T1_BRS are unused in non-FD operation
			std::uint32_t T0;
			std::uint32_t T1;
			// Message data are stored the same way as in RX FIFO elements. Refer to their comments
			std::uint32_t data[max_canfd_data_length / sizeof(std::uint32_t)];
		};

		/////////////////////////////////////////////////////////////////
		// Definition of bitmasks for interaction with TX Buffer elements
		/////////////////////////////////////////////////////////////////

		constexpr std::uint32_t T0_ESI_Msk = ufsel::bit::bit(31);
		constexpr std::uint32_t T0_XTD_Msk = ufsel::bit::bit(30);
		constexpr std::uint32_t T0_RTR_Msk = ufsel::bit::bit(29);
		constexpr std::uint32_t T0_ID_Msk_EXT = ufsel::bit::bitmask_between(28, 0);
		constexpr std::uint32_t T0_ID_Msk_STD = ufsel::bit::bitmask_between(28, 18);

		constexpr std::uint32_t T1_MM_Msk = ufsel::bit::bitmask_between(31, 24);
		constexpr std::uint32_t T1_EFC_Msk = ufsel::bit::bit(23);
		// T1 bit 22 reserved
		constexpr std::uint32_t T1_FDF_Msk = ufsel::bit::bit(21);
		constexpr std::uint32_t T1_BRS_Msk = ufsel::bit::bit(20);
		constexpr std::uint32_t T1_DLC_Msk = ufsel::bit::bitmask_between(19, 16);
		// T1 bits 15:0 reserved
	}

	namespace TX_event_FIFO {
		struct element {
			std::uint32_t E0;
			std::uint32_t E1;
		};

		/////////////////////////////////////////////////////////////////////
		// Definition of bitmasks for interaction with TX event FIFO elements
		/////////////////////////////////////////////////////////////////////

		constexpr std::uint32_t E0_ESI_Msk = ufsel::bit::bit(31);
		constexpr std::uint32_t E0_XTD_Msk = ufsel::bit::bit(30);
		constexpr std::uint32_t E0_RTR_Msk = ufsel::bit::bit(29);
		constexpr std::uint32_t E0_ID_Msk_EXT = ufsel::bit::bitmask_between(28, 0);
		constexpr std::uint32_t E0_ID_Msk_STD = ufsel::bit::bitmask_between(28, 18);

		constexpr std::uint32_t E1_MM_Msk = ufsel::bit::bitmask_between(31, 24);
		constexpr std::uint32_t E1_ET_Msk = ufsel::bit::bitmask_between(23, 22);
		constexpr std::uint32_t E1_EDL_Msk = ufsel::bit::bit(21);
		constexpr std::uint32_t E1_BRS_Msk = ufsel::bit::bit(20);
		constexpr std::uint32_t E1_DLC_Msk = ufsel::bit::bitmask_between(19, 16);
		constexpr std::uint32_t E1_TXTS_Msk = ufsel::bit::bitmask_between(15, 0);
	}

	namespace STD_Filter {
		struct element {
			std::uint32_t S0;
		};

		//////////////////////////////////////////////////////////////////////
		// Definition of bitmasks for interaction with Standard Filter element
		//////////////////////////////////////////////////////////////////////

		enum class SFT {
			Range = 0b00,
			DualID = 0b01,
			FilterWithMask = 0b10,
			Deactivated = 0b11,
		};

		enum class SFEC {
			Disable = 0b000,
			OnMatchStoreTo0 = 0b001,
			OnMatchStoreTo1 = 0b010,
			OnMatchReject = 0b011,
			OnMatchSetPriority = 0b100,
			PriorityOnMatchStoreTo0 = 0b101,
			PriorityOnMatchStoreTo1 = 0b110
		};

		constexpr std::uint32_t S0_SFT_Msk = ufsel::bit::bitmask_between(31, 30);
		constexpr std::uint32_t S0_SFEC_Msk = ufsel::bit::bitmask_between(29, 27);
		constexpr std::uint32_t S0_SFID1_Msk = ufsel::bit::bitmask_between(26, 16);
		constexpr std::uint32_t S0_SFID2_Msk = ufsel::bit::bitmask_between(10, 0);
	}

	namespace EXT_Filter {
		struct element {
			std::uint32_t F0;
			std::uint32_t F1;
		};

		//////////////////////////////////////////////////////////////////////
		// Definition of bitmasks for interaction with Extended Filter element
		//////////////////////////////////////////////////////////////////////

		enum class EFT {
			RangeWithXidam = 0b00,
			DualID = 0b01,
			FilterWithMask = 0b10,
			RangeNoXidam = 0b1,
		};

		enum class EFEC {
			Disable = 0b000,
			OnMatchStoreTo0 = 0b001,
			OnMatchStoreTo1 = 0b010,
			OnMatchReject = 0b011,
			OnMatchSetPriority = 0b100,
			PriorityOnMatchStoreTo0 = 0b101,
			PriorityOnMatchStoreTo1 = 0b110
		};

		constexpr std::uint32_t F0_EFEC_Msk = ufsel::bit::bitmask_between(31, 29);
		constexpr std::uint32_t F0_EFID1_Msk = ufsel::bit::bitmask_between(28, 0);
		constexpr std::uint32_t F1_EFTI_Msk = ufsel::bit::bitmask_between(31, 30);
		// F1 bit 29 not used
		constexpr std::uint32_t F1_EFID2_Msk = ufsel::bit::bitmask_between(28, 0);
	}

	struct MessageRAM_TypeDef {
		STD_Filter::element std_filter[28];
		EXT_Filter::element ext_filter[8];
		RX_FIFO::element rx_fifo0[3];
		RX_FIFO::element rx_fifo1[3];
		TX_event_FIFO::element tx_event_fifo[3];
		TX_Buffer::element tx_buffers[3];
	};


	// Sanity checks to make sure the memory mapping of Message RAM matches the hardware
	static_assert(offsetof(MessageRAM_TypeDef, std_filter) == 0);
	static_assert(offsetof(MessageRAM_TypeDef, ext_filter) == 0x70);
	static_assert(offsetof(MessageRAM_TypeDef, rx_fifo0) == 0xB0);
	static_assert(offsetof(MessageRAM_TypeDef, rx_fifo1) == 0x188);
	static_assert(offsetof(MessageRAM_TypeDef, tx_event_fifo) == 0x260);
	static_assert(offsetof(MessageRAM_TypeDef, tx_buffers) == 0x278);
	static_assert(sizeof(MessageRAM_TypeDef) == message_ram_size.toBytes());

	static_assert(sizeof(RX_FIFO::element) == 72);
	static_assert(sizeof(TX_Buffer::element) == 72);
	static_assert(sizeof(TX_event_FIFO::element) == 8);
	static_assert(sizeof(EXT_Filter::element) == 8);
	static_assert(sizeof(STD_Filter::element) == 4);

	// Periph num is 0 for FDCAN1, 1 for FDCAN 2 and 2 for FDCAN3
	constexpr std::uintptr_t message_ram_begin(int const periph_num) {
		return SRAMCAN_BASE + periph_num * message_ram_size.toBytes();
	}

	// Examples of Message RAM addresses given in the reference manual
	static_assert(message_ram_begin(0) == SRAMCAN_BASE);
	static_assert(message_ram_begin(1) == SRAMCAN_BASE + 0x350);

#define MessageRAM(fdcan_num) ((MessageRAM_TypeDef *) message_ram_begin(fdcan_num))
#define MessageRAM1 MessageRAM(0)
#define MessageRAM2 MessageRAM(1)
#define MessageRAM3 MessageRAM(2)

	inline int get_periph_num(FDCAN_GlobalTypeDef * const can) {
		switch (reinterpret_cast<std::uintptr_t>(can)) {
			case FDCAN1_BASE: return 0;
			case FDCAN2_BASE: return 1;
			case FDCAN3_BASE: return 2;
		}
		assert_unreachable();
	}

	inline MessageRAM_TypeDef * get_message_ram_for_periph(FDCAN_GlobalTypeDef * const can) {
		return MessageRAM(get_periph_num(can));
	}

	struct MessageData {
		std::uint32_t id, length;
		// Message data stored using little endian. First word has first message byte as its LSB
		std::array<std::uint32_t, max_canfd_data_length / sizeof(std::uint32_t)> data;
	};

	void Initialize(); //Fully initializes both CAN peripherals using configured clock frequencies.

	void insertMessageForTransmission(FDCAN_GlobalTypeDef * can, MessageData const& msg);

	//Returns true iff the given peripheral has at least one mailbox empty.
	[[nodiscard]]
	inline bool hasEmptyMailbox(FDCAN_GlobalTypeDef const* const can) {
		return not ufsel::bit::get(can->TXFQS, FDCAN_TXFQS_TFQF);
	}

	enum class LEC {
		NoError = 0b000,
		StuffError = 0b001,
		FormError = 0b010,
		AckError = 0b011,
		Bit1Error = 0b100,
		Bit0Error = 0b101,
		CRCError = 0b110,
		NoChange = 0b111
	};
	[[nodiscard]]
	inline LEC lastErrorCode(FDCAN_GlobalTypeDef * const can) {
		constexpr auto slice = ufsel::bit::slice::for_mask(FDCAN_PSR_LEC_Msk);
		return ufsel::bit::sliceable_value{ can->PSR }.decode_as<LEC>()[slice];
	};

	[[nodiscard]]
	inline bool hasAckError(FDCAN_GlobalTypeDef * const can) {
		return lastErrorCode(can) == LEC::AckError;
	};

}

#endif

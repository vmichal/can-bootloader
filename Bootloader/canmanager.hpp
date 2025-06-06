/*
 * eForce CAN Bootloader
 *
 * Written by Vojtěch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */

#pragma once

#include "flash.hpp"
#include "enums.hpp"

#include <optional>

namespace boot {

	void process_all_tx_fifos();

	class CanManager {

		Bootloader_Handshake_t lastSentHandshake_;

		std::optional<Bootloader_Handshake_t> pending_abort_request_;

	public:
		void set_pending_abort_request(Bootloader_Handshake_t abort_request) {
			assert(abort_request.Command == Bootloader_Command_AbortTransaction);
			pending_abort_request_ = abort_request;
		}

		void SendSoftwareBuild();
		void SendBeacon(Status const BLstate, EntryReason const entryReason);

		void SendDataAck(std::uint32_t address, WriteStatus result);
		void SendExitAck(bool exitPossible);
		void SendPingResponse(bool entering_bl);
		void SendHandshakeAck(Register reg, HandshakeResponse response, std::uint32_t val);
		void SendHandshake(Bootloader_Handshake_t const& msg);
		void SendTransactionMagic();
		void yieldCommunication();

		void RestartDataFrom(std::uint32_t address);

		Bootloader_Handshake_t const& lastSentHandshake() const { return lastSentHandshake_;}

		void update();
	};

	inline CanManager canManager;

}
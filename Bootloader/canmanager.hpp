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

namespace boot {

	class CanManager {

		Bootloader_Handshake_t lastSentHandshake_;

	public:
		void SendSoftwareBuild() const;
		void SendBeacon(Status const BLstate, EntryReason const entryReason) const;

		void SendDataAck(std::uint32_t address, WriteStatus result) const;
		void SendExitAck(bool exitPossible) const;
		void SendHandshakeAck(Register reg, HandshakeResponse response, std::uint32_t val) const;
		void SendHandshake(Register reg, Command command, std::uint32_t value);
		void SendHandshake(Bootloader_Handshake_t const& msg);
		void SendTransactionMagic() const;
		void yieldCommunication() const;

		void RestartDataFrom(std::uint32_t address);

		Bootloader_Handshake_t const& lastSentHandshake() const { return lastSentHandshake_;}
	};

}
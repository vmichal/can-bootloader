/*
 * eForce CAN Bootloader
 *
 * Written by Vojtìch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */

#pragma once

#include "flash.hpp"
#include "bootloader.hpp"

namespace boot {


	class CanManager {

		void SendSoftwareBuild() const;
		void SendSerialOutput() const;
		void SendBootloaderBeacon() const;

		template<int periph>
		static bool hasEmptyMailbox();

		Bootloader const& bootloader_;

	public:
		void Update();

		void SendDataAck(std::uint32_t, WriteStatus) const;
		void SendExitAck(bool) const;
		void SendHandshakeAck(Register reg, HandshakeResponse response, std::uint32_t val) const;

		void FlushSerialOutput() const;

		CanManager(Bootloader const& bootloader) : bootloader_{bootloader} {}

	};

}
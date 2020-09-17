/*
 * eForce CAN Bootloader
 *
 * Written by Vojtìch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */

#pragma once

namespace boot {


	class CanManager {

		void SendSoftwareBuild() const;
		void SendSerialOutput() const;

		template<int periph>
		static bool hasEmptyMailbox();

	public:
		void Update();


	};

}
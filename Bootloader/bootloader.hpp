/*
 * eForce CAN Bootloader
 *
 * Written by Vojtìch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */

#pragma once

#include <cstdint>
#include <array>
#include <type_traits>

#include <library/assert.hpp>

#include "flash.hpp"
#include "can_Bootloader.h"

namespace boot {

	enum class EntryReason {
		DontEnter, //Bootloader will not be entered
		InvalidMagic, //Either no firmware is flashed, or there has been a memory corruption
		UnalignedInterruptVector, //The application's interrupt vector is not aligned properly
		InvalidEntryPoint, //The entry point pointer does not point into flash

		backupRegisterCorrupted, //The backup register contained value different from 0 (reset value) or application_magic
		Requested, //The bootloader was requested
	};

	inline EntryReason bootloadeEntryReason;

	enum class Register {
		EntryPoint,
		InterruptVector,
		NumPagesToErase,
		PageToErase,
		FirmwareSize,
		TransactionMagic
	};

	constexpr Bootloader_Register regToReg(Register r) {
		switch (r) {
		case Register::EntryPoint:
			return Bootloader_Register_EntryPoint;
		case Register::InterruptVector:
			return Bootloader_Register_InterruptVector;
		case Register::NumPagesToErase:
			return Bootloader_Register_NumPagesToErase;
		case Register::PageToErase:
			return Bootloader_Register_PageToErase;

		case Register::FirmwareSize:
			return Bootloader_Register_FirmwareSize;
		case Register::TransactionMagic:
			return Bootloader_Register_TransacionMagic;
		}
		assert_unreachable();
	}
	constexpr Register regToReg(Bootloader_Register r) {
		switch (r) {
		case Bootloader_Register_EntryPoint:
			return Register::EntryPoint;
		case Bootloader_Register_InterruptVector:
			return Register::InterruptVector;
		case Bootloader_Register_NumPagesToErase:
			return Register::NumPagesToErase;
		case Bootloader_Register_PageToErase:
			return Register::PageToErase;
		case Bootloader_Register_FirmwareSize:
			return Register::FirmwareSize;
		case Bootloader_Register_TransacionMagic:
			return Register::TransactionMagic;
		}
		assert_unreachable();
	}

	constexpr char const* to_string(Bootloader_BootTarget target) {
		switch (target) {
		case Bootloader_BootTarget_AMS: return "AMS";
		}
		return "UNKNOWN";
	}

	constexpr char const* explain_enter_reason(EntryReason reason) {
		switch (reason) {
		case EntryReason::backupRegisterCorrupted:
			return "BKP register corrupted.";
		case EntryReason::DontEnter:
			return "Don't enter.";
		case EntryReason::InvalidEntryPoint:
			return "App entry point invalid.";
		case EntryReason::InvalidMagic:
			return "Invalid jump table magic.";
		case EntryReason::Requested:
			return "Bootloader requested.";
		case EntryReason::UnalignedInterruptVector:
			return "App isr vector not aligned.";
		}
		return "UNKNOWN";
	}



	enum class HandshakeResponse {
		Ok,
		PageAddressNotAligned,
		AddressNotInFlash,
		PageProtected,
		ErasedPageCountMismatch
	};


	class Bootloader {
	public:
		enum class Status {
			Ready,
			TransactionStarted,
			ReceivedFirmwareSize,
			ReceivedNumPagestoErase,
			ErasingPages,
			ReceivedEntryPoint,
			ReceivedInterruptVector,
			ReceivingData,
			Error
		};

	private:
		std::array<std::uint32_t, Flash::pageCount> erased_pages_;
		std::size_t erased_pages_count_;
		Status status_ = Status::Ready;
		static inline EntryReason entryReason_ = EntryReason::DontEnter;

		WriteStatus checkBeforeWrite(std::uint32_t address);


	public:
		constexpr static Bootloader_BootTarget thisUnit = Bootloader_BootTarget_AMS;
		Status status() const { return status_; }

		static void resetToApplication();

		WriteStatus write(std::uint32_t address, std::uint16_t half_word);
		WriteStatus write(std::uint32_t address, std::uint32_t word);


		HandshakeResponse processHandshake(Register reg, std::uint32_t value);

		static void setEntryReason(EntryReason);
		static EntryReason entryReason() { return entryReason_; }
	};

}


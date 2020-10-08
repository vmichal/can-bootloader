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
#include <library/units.hpp>

#include "flash.hpp"
#include "can_Bootloader.h"

namespace boot {

	enum class EntryReason {
		DontEnter, //Bootloader will not be entered (jump straight to application)
		InvalidMagic, //Either no firmware is flashed, or there has been a memory corruption
		UnalignedInterruptVector, //The application's interrupt vector is not aligned properly
		InvalidEntryPoint, //The entry point pointer does not point into flash
		InvalidInterruptVector, //The vector table pointer not point into flash
		EntryPointMismatch, //The entry point is not the same as the second word of interrupt vector
		InvalidTopOfStack, //The specified top of stack points to flash

		backupRegisterCorrupted, //The backup register contained value different from 0 (reset value) or application_magic
		Requested, //The bootloader was requested
		ApplicationReturned, //The application returned from main
	};

	inline EntryReason bootloadeEntryReason;

	enum class Register {
		EntryPoint,
		InterruptVector,
		NumPagesToErase,
		PageToErase,
		FirmwareSize,
		Checksum,
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
		case Register::Checksum:
			return Bootloader_Register_Checksum;
		case Register::TransactionMagic:
			return Bootloader_Register_TransactionMagic;
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
		case Bootloader_Register_Checksum:
			return Register::Checksum;
		case Bootloader_Register_TransactionMagic:
			return Register::TransactionMagic;
		}
		assert_unreachable();
	}

	enum class HandshakeResponse {
		Ok,
		PageAddressNotAligned,
		AddressNotInFlash,
		PageProtected,
		ErasedPageCountMismatch,
		BinaryTooBig,
		InterruptVectorNotAligned,
		InvalidTransactionMagic,
		HandshakeSequenceError,
		PageAlreadyErased,
		NotEnoughPages,
		NumWrittenBytesMismatch,
		EntryPointAddressMismatch,
		ChecksumMismatch,
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
			ReceivedChecksum,
			Error
		};



	private:
		struct FirmwareData {
			InformationSize expectedBytes_ = 0_B; //Set during the handshake. Number of bytes to be written
			InformationSize writtenBytes_ = 0_B; //The current number of bytes written. It is expected to equal the value expectedBytes_.
			std::uint32_t entryPoint_ = 0; //Address of the entry point of flashed firmware
			std::uint32_t interruptVector_ = 0; //Address of the interrupt table of flashed firmware
			std::uint32_t numPagesToErase_ = 0; //Set during the handshake. Number of flash pages to erase
			std::uint32_t checksum_ = 0; //Checksum of already received words
		};

		std::array<std::uint32_t, Flash::pageCount> erased_pages_;
		std::size_t erased_pages_count_;
		Status status_ = Status::Ready;
		FirmwareData firmware_;
		static inline EntryReason entryReason_ = EntryReason::DontEnter;
		static inline int appErrorCode_ = 0;

		WriteStatus checkAddressBeforeWrite(std::uint32_t address);

		//Sets the jumpTable
		void finishTransaction() const;

		constexpr static auto magic_ = "Heli";
	public:
		constexpr static std::uint32_t transactionMagic = magic_[0] << 24 | magic_[1] << 16 | magic_[2] << 8 | magic_[3];


		constexpr static Bootloader_BootTarget thisUnit = Bootloader_BootTarget_AMS;
		Status status() const { return status_; }

		[[noreturn]]
		static void resetToApplication();
		HandshakeResponse tryErasePage(std::uint32_t address);

		WriteStatus write(std::uint32_t address, std::uint16_t half_word);
		WriteStatus write(std::uint32_t address, std::uint32_t word);


		HandshakeResponse processHandshake(Register reg, std::uint32_t value);

		static void setEntryReason(EntryReason, int appErrorCode = 0);

		static int appErrorCode() { return appErrorCode_; }
		static EntryReason entryReason() { return entryReason_; }
	};

	static_assert(Bootloader::transactionMagic == 0x48656c69); //This value is stated in the protocol description

}


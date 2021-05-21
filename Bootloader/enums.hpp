/*
 * eForce CAN Bootloader
 *
 * Written by Vojtech Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */

#pragma once
#include <can_Bootloader.h>

namespace boot {

	enum class Register {
		EntryPoint = Bootloader_Register_EntryPoint,
		InterruptVector = Bootloader_Register_InterruptVector,
		NumPhysicalBlocksToErase = Bootloader_Register_NumPhysicalBlocksToErase,
		PhysicalBlockToErase = Bootloader_Register_PhysicalBlockToErase,
		FirmwareSize = Bootloader_Register_FirmwareSize,
		Checksum = Bootloader_Register_Checksum,
		TransactionMagic = Bootloader_Register_TransactionMagic,
		NumLogicalMemoryBlocks = Bootloader_Register_NumLogicalMemoryBlocks,
		LogicalBlockStart = Bootloader_Register_LogicalBlockStart,
		LogicalBlockLength = Bootloader_Register_LogicalBlockLength,
		NumPhysicalMemoryBlocks = Bootloader_Register_NumPhysicalMemoryBlocks,
		PhysicalBlockStart = Bootloader_Register_PhysicalBlockStart,
		PhysicalBlockLength = Bootloader_Register_PhysicalBlockLength,
		Command = Bootloader_Register_Command,
	};

	enum class Command {
		None = Bootloader_Command_None,
		StartTransactionFlashing = Bootloader_Command_StartTransactionFlashing,
		AbortTransaction = Bootloader_Command_AbortTransaction,
		StallSubtransaction = Bootloader_Command_StallSubtransaction,
		ResumeSubtransaction = Bootloader_Command_ResumeSubtransaction,
		RestartFromAddress = Bootloader_Command_RestartFromAddress,
		StartBootloaderUpdate = Bootloader_Command_StartBootloaderUpdate,
		SetNewVectorTable = Bootloader_Command_SetNewVectorTable,
	};

	enum class HandshakeResponse {
		Ok = Bootloader_HandshakeResponse_OK,
		PageAddressNotAligned = Bootloader_HandshakeResponse_PageAddressNotAligned,
		AddressNotInFlash = Bootloader_HandshakeResponse_AddressNotInFlash,
		PageProtected = Bootloader_HandshakeResponse_PageProtected,
		ErasedPageCountMismatch = Bootloader_HandshakeResponse_ErasedPageCountMismatch,
		BinaryTooBig = Bootloader_HandshakeResponse_BinaryTooBig,
		InterruptVectorNotAligned = Bootloader_HandshakeResponse_InterruptVectorNotAligned,
		InvalidTransactionMagic = Bootloader_HandshakeResponse_InvalidTransactionMagic,
		HandshakeSequenceError = Bootloader_HandshakeResponse_HandshakeSequenceError,
		PageAlreadyErased = Bootloader_HandshakeResponse_PageAlreadyErased,
		NotEnoughPages = Bootloader_HandshakeResponse_NotEnoughPages,
		NumWrittenBytesMismatch = Bootloader_HandshakeResponse_NumWrittenBytesMismatch,
		EntryPointAddressMismatch = Bootloader_HandshakeResponse_EntryPointAddressMismatch,
		ChecksumMismatch = Bootloader_HandshakeResponse_ChecksumMismatch,
		TooManyLogicalMemoryBlocks = Bootloader_HandshakeResponse_TooManyLogicalMemoryBlocks,
		UnknownTransactionType = Bootloader_HandshakeResponse_UnknownTransactionType,
		HandshakeNotExpected = Bootloader_HandshakeResponse_HandshakeNotExpected,
		InternalStateMachineError = Bootloader_HandshakeResponse_InternalStateMachineError,
		CommandNotNone = Bootloader_HandshakeResponse_CommandNotNone,
		BootloaderInError = Bootloader_HandshakeResponse_BootloaderInError,
		CommandInvalidInCurrentContext = Bootloader_HandshakeResponse_CommandInvalidInCurrentContext,
		LogicalBlockCountMismatch = Bootloader_HandshakeResponse_LogicalBlockCountMismatch,
		LogicalBlocksOverlapping = Bootloader_HandshakeResponse_LogicalBlocksOverlapping,
		LogicalBlockAddressesNotIncreasing = Bootloader_HandshakeResponse_LogicalBlockAddressesNotIncreasing,
		LogicalBlockNotCoverable = Bootloader_HandshakeResponse_LogicalBlockNotCoverable,
		LogicalBlockNotInFlash = Bootloader_HandshakeResponse_LogicalBlockNotInFlash,
		LogicalBlockTooLong = Bootloader_HandshakeResponse_LogicalBlockTooLong,
	};

	enum class EntryReason {

		DontEnter = Bootloader_EntryReason_DontEnter, //Bootloader will not be entered (jump straight to application)
		InvalidMagic = Bootloader_EntryReason_InvalidMagic, //Either no firmware is flashed, or there has been a memory corruption
		UnalignedInterruptVector = Bootloader_EntryReason_UnalignedInterruptVector, //The application's interrupt vector is not aligned properly
		InvalidEntryPoint = Bootloader_EntryReason_InvalidEntryPoint, //The entry point pointer does not point into flash
		InvalidInterruptVector = Bootloader_EntryReason_InvalidInterruptVector, //The vector table pointer not point into flash
		InvalidTopOfStack = Bootloader_EntryReason_InvalidTopOfStack, //The specified top of stack points to flash
		BackupRegisterCorrupted = Bootloader_EntryReason_BackupRegisterCorrupted, //The backup register contained value different from 0 (reset value) or application_magic
		Requested = Bootloader_EntryReason_Requested, //The bootloader was requested
		ApplicationFailure = Bootloader_EntryReason_ApplicationFailure, //The application died horribly.
	};

	enum class Status {
		/* Bootloader is ready to commence handshake with the flashing system */
		Ready = Bootloader_State_Ready,
		/* Bootloader received transaction magic and initialization is ongoing. */
		Initialization = Bootloader_State_Initialization,
		/* BL is sending the available memory map */
		TransmittingPhysicalMemoryBlocks = Bootloader_State_TransmittingPhysicalMemoryBlocks,
		/* BL is receiving the memory map of new firmware */
		ReceivingFirmwareMemoryMap = Bootloader_State_ReceivingFirmwareMemoryMap,
		/* BL is erasing physical blocks */
		ErasingPhysicalBlocks = Bootloader_State_ErasingPhysicalBlocks,
		/* BL is flashing new firmware */
		DownloadingFirmware = Bootloader_State_DownloadingFirmware,
		/* BL is receiving entry point/isr vector */
		ReceivingFirmwareMetadata = Bootloader_State_ReceivingFirmwareMetadata,
		/* Some error occured. //TODO make it more concrete */
		Error = Bootloader_State_Error,
		/* Bootloader is overwhelmed and has suspended the communication */
		ComunicationStalled = Bootloader_State_CommunicationStalled
	};

}

/* Memory map for bootloader aware firmware

* Written by Vojtech Michal
* Copyright (c) eForce FEE Prague Formula, 2020, 2021

This file defines some memory requiremets shared among all MCU generations.
This is an internal file and hence shall not be included into any other linker script outside of bootloader project! */

/* CompleteBootloaderSize and JumpTableSize shall already be defined by the including file. */

BootloaderMetadataSize = 512;
BootloaderVectorSize = 512;

BootloaderSize = CompleteBootloaderSize - BootloaderMetadataSize - BootloaderVectorSize;

/* FlashStart should already be defined by the including script. */
BootloaderFlashStart = FlashStart + BootloaderVectorSize + BootloaderMetadataSize;

/* Specify common memory areas */
MEMORY
{
BootloaderISR(rx)        : ORIGIN = FlashStart, LENGTH = BootloaderVectorSize
BootloaderMetadata(r)    : ORIGIN = FlashStart + BootloaderVectorSize, LENGTH = BootloaderMetadataSize

BootloaderFlash(rx)      : ORIGIN = BootloaderFlashStart, LENGTH = BootloaderSize
JumpTableFlash(rx)       : ORIGIN = BootloaderFlashStart + BootloaderSize, LENGTH = JumpTableSize
ApplicationFlash(rx)     : ORIGIN = BootloaderFlashStart + BootloaderSize + JumpTableSize, LENGTH = FlashSize - BootloaderSize - JumpTableSize
}

bootloader_metadata_address = ORIGIN(BootloaderMetadata)

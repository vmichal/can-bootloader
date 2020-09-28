
/* Memory map for bootloader aware firmware

* Written by Vojtech Michal
* Copyright (c) eForce FEE Prague Formula, 2020

Include this file in your linker script to get the memory map for bootloader aware application. */

FlashStart = 0x08000000;
FlashSize = 256K;
BootloaderSize = 14K;
JumpTableSize = 2K;

/* Specify the memory areas */
MEMORY
{
BootloaderFlash(rx)      : ORIGIN = FlashStart, LENGTH = BootloaderSize
JumpTableFlash(rx)       : ORIGIN = FlashStart + BootloaderSize, LENGTH = JumpTableSize
ApplicationFlash(rx)     : ORIGIN = FlashStart + BootloaderSize + JumpTableSize, LENGTH = FlashSize - BootloaderSize - JumpTableSize
RAM (xrw)                : ORIGIN = 0x20000000, LENGTH = 64K
}

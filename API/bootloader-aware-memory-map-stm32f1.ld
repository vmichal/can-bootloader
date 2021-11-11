
/* Memory map for bootloader aware firmware

* Written by Vojtech Michal
* Copyright (c) eForce FEE Prague Formula, 2020, 2021

Include this file in your linker script to get the memory map for bootloader aware application. */

/* These two constants constitute the BL's memory customization point.
Fill in values from your reference manual. */
FlashStart = 0x08000000;
FlashSize = 256K;

/* 16bit memory location in the backup domain used to exchange information
between BL and application during system resets.*/
BootControlBackupRegisterAddress = 0x40006c00 + 4; /* BKP domain, reg one used.*/

/* BL's requirements. 14K for BL code and 2k for firmware jump table.*/
CompleteBootloaderSize = 14K;
JumpTableSize = 2K;

INCLUDE ../../API/bootloader-aware-memory-map-shared.ld

/* Specify the memory areas */
MEMORY
{
RAM (xrw)                : ORIGIN = 0x20000000, LENGTH = 64K
}


/* Memory map for bootloader aware firmware

* Written by Vojtech Michal
* Copyright (c) eForce FEE Prague Formula, 2020, 2021

Include this file in your linker script to get the memory map for bootloader aware application. */

/* These two constants constitute the BL's memory customization point.
Fill in values from your reference manual. */
FlashStart = 0x08000000;
FlashSize = 1M;

/* 16bit memory location in the backup domain used to exchange information
between BL and application during system resets.*/
BootControlBackupRegisterAddress = 0x40002800 + 0x50; /* BKP domain (in RTC), reg one used.*/

/* BL's requirements. 16K for BL code and 16k for firmware jump table (each is a separate sector).*/
CompleteBootloaderSize = 16K;
JumpTableSize = 16K;

INCLUDE ../../API/bootloader-aware-memory-map-shared.ld

/* Specify the memory areas */
MEMORY
{
RAM (xrw)                : ORIGIN = 0x20001000, LENGTH = 252K /*TODO this is a temporary workaround for error in FSEX.PDL SRAM (cell 0x20'00'04cc cannot be written by the MCU)*/
}

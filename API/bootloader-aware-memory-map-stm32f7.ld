
/* Memory map for bootloader aware firmware running on STM32F7

* Written by Vojtech Michal
* Copyright (c) eForce FEE Prague Formula, 2020, 2021

Include this file in your linker script to get the memory map for bootloader aware application. */

/* These two constants constitute the BL's memory customization point.
Fill in values from your reference manual. */
FlashStart = 0x08000000 + 64K; /* 2 pages are allocated to the flash file system of the steering wheel */
FlashSize = 1M - 64K;

/* 16bit memory location in the backup domain used to exchange information
between BL and application during system resets.*/
BootControlBackupRegisterAddress = 0x40002800 + 0x50; /* BKP domain (in RTC), reg one used.*/

/* BL's requirements. 32K for BL code and 32k for firmware jump table (each is a separate sector).*/
CompleteBootloaderSize = 32K;
JumpTableSize = 32K;

/* Specify the memory areas */
MEMORY
{
RAM   (xrw)              : ORIGIN = 0x20000000, LENGTH = 128K /*DTCM*/
SRAM1 (xrw)              : ORIGIN = 0x20020000, LENGTH = 368K
SRAM2 (xrw)              : ORIGIN = 0x2007C000, LENGTH = 16K
}

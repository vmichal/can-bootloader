
/* Memory map for bootloader aware firmware

* Written by Vojtech Michal
* Copyright (c) eForce FEE Prague Formula, 2021

Include this file in your linker script to get the memory map for bootloader aware application. */

/* These two constants constitute the BL's memory customization point.
Fill in values from your reference manual. */
FlashStart = 0x08000000;
FlashSize = 512K;

/* 32bit memory locations in the backup domain (located in TAMP peripheral) used to exchange information
between BL and application during system resets.*/
BootControlBackupRegisterAddress = 0x40002400 + 0x100; /* BKP domain (in TAMP), reg one used.*/

/* BL's requirements. 12K for BL code and 4k for firmware jump table.*/
/* TODO the bootloader shall not be this big. Investigate why it needs 18K of memory*/
CompleteBootloaderSize = 20K;
JumpTableSize = 2K;



/* Specify the memory areas */
MEMORY
{

/* Use only SRAM1, here called RAM*/
RAM (xrw)              : ORIGIN = 0x20000000, LENGTH = 80K
SRAM2 (xrw)              : ORIGIN = 0x20014000, LENGTH = 16K
CCM_RAM   (xrw)              : ORIGIN = 0x20018000, LENGTH = 16K /*CCM*/
}

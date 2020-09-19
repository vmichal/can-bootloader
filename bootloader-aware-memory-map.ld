

FlashStart = 0x08000000;
FlashSize = 256K;
BootloaderSize = 18K;
ApplicationJumpTableSize = 2K;


/* Specify the memory areas */
MEMORY
{
FLASH(rx)                : ORIGIN = FlashStart, LENGTH = BootloaderSize
JumpTableFlash(rx)       : ORIGIN = FlashStart + BootloaderSize, LENGTH = ApplicationJumpTableSize
ApplicationFlash(rx)     : ORIGIN = FlashStart + BootloaderSize + ApplicationJumpTableSize, LENGTH = FlashSize - BootloaderSize
RAM (xrw)                : ORIGIN = 0x20000000, LENGTH = 64K
}

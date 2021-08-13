
eForce CAN Bootloader integration procedure
===================

Written by Vojtěch Michal - vojta.michall@email.cz, Discord `Vojtěch Michal#7362`<br/>
eForce FEE Prague Formula, 2021

This document will lead you through the process of making your application bootloader-aware.
Supported MCUs currently include ST Microelectronics F1, F4, F7.

## Prerequisites 
Make sure you meet these requirements:
  - C++20 toolchain (GCC 10 and higher)
  - Ability to modify your linker script
  - UFSEL is already a submodule of yours and you use `UFSEL/Assertion` and `UFSEL/BitOperations`.


## Detailed procedure guide
1. Add the Bootloader repository as a submodule of your project.
    1. Run `git submodule add https://eforce1.feld.cvut.cz/gitlab/vomi/bootloader.git` in your project repo.
       Unless you have really specific needs (in which case you don't need this guide), put the submodule into the same
       directory where the rest of submodules reside. This is normally the project root. In any case, you must be able to
       `#include` a file from BL repository. In the end, you should see a non-empty directory `bootloader` in your filesystem view.
    2. If your project's remote is `eforce1.feld.cvut.cz` (which is obligatory for all eForce projects), be sure to manually
       edit te file `.gitmodules` located in the root of your repository. This human-readable text file contains a single
       record for each submodule referenced by your repo. The field `url` normally uses absolute url, but submodules located
       on the same remote must use a relative url. In practice, you wan't the corresponding record in .gitmodules to look like this
       ```
       [submodule "bootloader"]
        path = bootloader
        url = ../../vomi/bootloader.git
       ```
    3. **CHECKPOINT**: You may commit and push the repository. If your CI pipeline does not fail due to name resolution error
       or something similar, you have done it correct.
2. Modify CANdb. Two actions will be needed, one in the bootloader specification and one in your unit. In case your unit
   does not normally transmit CAN messages, lacks CANdb record or there is anything else that would hinder you from proceeding,
   consult it with someone experienced.
    1. Navigate to unit `Accessory::Bootloader`, open enumeration `BootTarget`, take a look at constants already present
       (those currently include AMS, PDL, DSH, FSB etc). If none of them could be used to meaningfully identify your unit,
       add a new constant. Do **not** prepend the name with formula generation (e.g. FSEX_AMS), use only the unit's name instead.
       This rule simplifies many things, mostly the actual process of executing a bootloader transaction and transmitting
       a new firmware over the CAN bus.
    2. Check the overview page of `Accessory::Bootloader`, specifically the list of used Buses. Make sure it contains
       all buses listed in the overview page of formula generation (package) your unit belongs to. e.g. if you are 
       in the process of adding BL to a unit affiliated with D1, make sure that the bootloader contains reference to
       both `CAN3` and `CAN1_powertrain`. If it doesn't, add a reference to it.
    3. Navigate to your unit (e.g. FSE10::DSH) and enter edit mode. Add message `Ping` owned by `Accessory::Bootloader`
       to the list of received messages. Similarly add `PingResponse` to the list of sent messages. Click save.
3. (OPTIONAL, can be performed later, but works as a verification of previous steps)
    1. Generate CAN code for your unit in accordance with recent CANdb changes (prefer candb-dev). Add it to your project.
    2. **CHECKPOINT**: Open header/source files describing the CAN protocol. It should contain the definition of messages
       you've just generated.
    3. Download JSON v2 (for the whole package containing your unit, the same JSON you use for pycandb or CAN-GUI).
    4. **CHECKPOINT**: Open the JSON and make sure it contains the unit Accessory::Bootloader.
4. Firmware integration. These steps can generally be performed in any order as there are no checkpoints on the way
   except for the final one - it works. The order presented here is not special neither recommended in any way.
    1.  Open your project's `CMakeLists.txt`.
        1. Add `bootloader/API/BLdriver.cpp` to the list of source files (must be compiled with your program).
           This addition may not be possible using the line `bootloader/API/BLdriver.cpp` verbatim should you
           decide not to put the bootloader submodule into the project root. 
        2. Modify your include path such that bootloader code is accessible using `#include` directives.
        3. **Note**: The recommended setup places the bootloader submodule into the root of project tree
           (and consequently to cmake root), so that bootloader files are accessible
           using `#include <bootloader/API/BLdriver.hpp>`.
    2. Modify your CMake toolchain file(s), i.e. files with extension `xxxxxxx.cmake`.
        1. You need to specify a define `BOOT_STM32Fx`, where x is one of {1,2,4,7} and correspondsto the MCU family
           you use. This macro need not have a value as only its presence/absence is tested. The recommended way of defining
           this macro is by adding `add_definitions(-DBOOT_STM32F_)` to `CMakeLists.txt`
        2. You may need to add defines to satisfy the requirements of peripheral drivers used by the bootloader.
           TODO This step should be more concrete when more experience is gathered. Errors originating from this point
           have had vastly different causes so far, therefore it is best to consult with someone when you get stuck.
    3. Modify your linker file, i.e. `xxxxxxx.ld`
        1. Add statement `INCLUDE ../bootloader/API/bootloader-aware-memory-map-stm32f_.ld` with underscore in `stm32f_` replaced.
           Note the double dot denoting the parent directory. You may need to go up more than one level depending on the directory
           structure you use for building. Think of it like this - you specify a linker script (usually in your cmake toolchain file).
           The linker is later given this script to execute when the compiler has converted all source files to object files.
           The linker is invoked from your build root, which is usually `{Project root}/build`. The path used to reference the linker
           script located in BL repository must respect this. If you are building a binary in `{Project root}/build/FSE10`,
           then you need to go two levels up with `INCLUDE ../../bootloader/`.
        2. Bootloader aware memory map will define (possibly among others) memory areas `RAM` and `ApplicationFlash`.
           Don't worry about the exact origin or length, they will be as big as possible. They span the whole flash and RAM memory
           except for blocks required by the bootloader itself to store firmware metadata and BL code.
           Rewrite your statement `MEMORY` referencing these two memory areas.
           An example could be the following part of AMS linker script
           ```
           MEMORY
           {
           FLASH (rx)                 : ORIGIN = ORIGIN(ApplicationFlash), LENGTH = LENGTH(ApplicationFlash) - 10K - 2K - 64K
           FLASH_LongTermDataBackup   : ORIGIN = ORIGIN(FLASH) + LENGTH(FLASH), LENGTH = 64K
           FLASH_ShortTermDataBackup  : ORIGIN = ORIGIN(FLASH_LongTermDataBackup) + LENGTH(FLASH_LongTermDataBackup), LENGTH = 10K
           FLASH_LastPage(rw)         : ORIGIN = ORIGIN(FLASH_ShortTermDataBackup) + LENGTH(FLASH_ShortTermDataBackup), LENGTH = 2K
           }
           ```
    4. Modify application code. Your firmware will interact with the flash master using messages `Ping` and `PingResponse`.
        1. If you use CAN filters, allow `PingResponse` through.
        2. Implement logic similar to the one shown in file integration_example.txt. You need to detect reception of
           this message (the simplest way is a callback) and react to it. If the target does not match the unit identifier,
           ignore the mesage, Oterwise if the bootloader is not requested, transmit a response with `BootloaderPending == false`.
           Otherwise check whether there are no critical processes running in your FW (e.g. active TS) and if so,
           refuse to enter the bootloader. Otherwise if not, transmit response with `BootloaderPending == true`
           and reboot into the bootloder.
        3. Perform a global search for accesses to System Control Block Vector Table Offset Register `SCB->VTOR`.
           They may interfere with bootloader's operation. Consult any occurrence with another experienced programmer.
    5. **CHECKPOINT**: You should be able to build the project now. After successful compilation, disassemble the binary
       and check that the isr vector is not located at `0x0800'0000` but rather several KiB above the start of flash memory.
       STM32F1 bootloader reserves 12 KiB of memory (therefore the application isr vector should be located at 0x0800'3000),
       bootloaders for f2, f4 and f7 reserve two pages (32 KiB of flash, therefore the application's isr vector starts at 0x0800'8000).
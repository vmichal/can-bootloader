/*
    Bootloader linker script for STM32G4

    Vojtech Michal, eForce FEE Prague Formula, 2021
*/

/*Includes a MEMORY command defining address space available to the application*/
INCLUDE ../../API/bootloader-aware-memory-map-stm32g4.ld
INCLUDE ../../API/bootloader-aware-memory-map-shared.ld

/* Contains SECTIONS command etc */
INCLUDE ../../shared-linker.ld

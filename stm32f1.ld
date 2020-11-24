/*
*****************************************************************************
**
**  File        : LinkerScript.ld
**
**  Abstract    : Linker script for STM32F105xC Device with
**                256KByte FLASH, 64KByte RAM
**
**                Set heap size, stack size and stack location according
**                to application requirements.
**
**                Set memory bank area and size if external memory is used.
**
**  Target      : STMicroelectronics STM32
**
**  Environment : System Workbench for MCU
**
**  Distribution: The file is distributed as is, without any warranty
**                of any kind.
**
*****************************************************************************
**
** <h2><center>&copy; COPYRIGHT(c) 2014 Ac6</center></h2>
**
** Redistribution and use in source and binary forms, with or without modification,
** are permitted provided that the following conditions are met:
**   1. Redistributions of source code must retain the above copyright notice,
**      this list of conditions and the following disclaimer.
**   2. Redistributions in binary form must reproduce the above copyright notice,
**      this list of conditions and the following disclaimer in the documentation
**      and/or other materials provided with the distribution.
**   3. Neither the name of Ac6 nor the names of its contributors
**      may be used to endorse or promote products derived from this software
**      without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\"
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
** OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
*****************************************************************************
*/

/* Entry Point */
ENTRY(Reset_Handler)


/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size = 0;      /* required amount of heap. None is required right now*/
_Min_Stack_Size = 0x800; /* required amount of stack, at least 2 KiB */

/*Includes a MEMORY command defining address space available to the application*/
INCLUDE ../API/bootloader-aware-memory-map-stm32f1.ld

bootloader_flash_start = ORIGIN(BootloaderFlash);
bootloader_flash_end = ORIGIN(BootloaderFlash) + LENGTH(BootloaderFlash);

jumpTable_start = ORIGIN(JumpTableFlash);
jumpTable_end = ORIGIN(JumpTableFlash) + LENGTH(JumpTableFlash);

available_flash_start = ORIGIN(ApplicationFlash);
available_flash_end = ORIGIN(ApplicationFlash) + LENGTH(ApplicationFlash);

ram_start = ORIGIN(RAM);
ram_end = ORIGIN(RAM) + LENGTH(RAM);

/* Highest address of the user mode stack */
_estack = ORIGIN(RAM) + LENGTH(RAM); /* end of RAM */

/* Define output sections */
SECTIONS
{

  _load_isr_vector = LOADADDR(.isr_vector);
  /* The startup code goes first into BootloaderFlash */
  .isr_vector :
  {
    /*Cortex M3 requires the interrupt vector be aligned to 512B boundary if we wanted to relocate it. */
    . = ALIGN(1 << 9);
    _sisr_vector = .;
    KEEP(*(.isr_vector)) /* Startup code */
    _eisr_vector = .;
    . = ALIGN(4);
  } > RAM AT> BootloaderFlash

  _load_text = LOADADDR(.text);
  /* The program code and other data goes into BootloaderFlash */
  .text :
  {
    . = ALIGN(4);
    _stext = .;
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } > RAM AT> BootloaderFlash

    /* .ctors .dtors are used for c++ constructors/destructors */
    .ctors :
    {
    PROVIDE(__ctors_start__ = .);
    KEEP(*(SORT(.ctors.*)))
    KEEP(*(.ctors))
    PROVIDE(__ctors_end__ = .);
    } >BootloaderFlash

    .dtors :
    {
    PROVIDE(__dtors_start__ = .);
    KEEP(*(SORT(.dtors.*)))
    KEEP(*(.dtors))
    PROVIDE(__dtors_end__ = .);
    } >BootloaderFlash

    .executed_from_flash : {
     *(.executed_from_flash*)
    } >BootloaderFlash

  _load_rodata = LOADADDR(.rodata);
  /* Constant data goes into BootloaderFlash */
  .rodata :
  {
    . = ALIGN(4);
    _srodata = .;
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    _erodata = .;
    . = ALIGN(4);
  } > RAM AT> BootloaderFlash

  .ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >BootloaderFlash
  .ARM : {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >BootloaderFlash

  .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >BootloaderFlash
  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >BootloaderFlash
  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >BootloaderFlash

  /* used by the startup to initialize data */
  _load_data = LOADADDR(.data);

  /* Initialized data sections goes into RAM, load LMA copy after code */
  .data : 
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */

    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
  } >RAM AT> BootloaderFlash

  
  /* Uninitialized data section */
  . = ALIGN(4);
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss secion */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >RAM

  /* User_heap_stack section, used to check that there is enough RAM left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  .jumpTableSection(NOLOAD) : 
  {
   . = ALIGN(4);
   KEEP(*(jumpTableSection))
  } > JumpTableFlash


  /* Remove information from the standard libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}

/*
    Bootloader linker script for STM32F4

    Vojtech Michal, eForce FEE Prague Formula, 2021
*/

/* Entry Point */
ENTRY(Reset_Handler)


/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size = 0;      /* required amount of heap. None is required right now*/
_Min_Stack_Size = 0x800; /* required amount of stack, at least 2 KiB */

/*Includes a MEMORY command defining address space available to the application*/
INCLUDE ../../API/bootloader-aware-memory-map-stm32f2.ld

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

/* Minimal linker script for MSPM0G3507 (128 KB flash, 32 KB SRAM). */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 128K
    RAM   (rwx) : ORIGIN = 0x20200000, LENGTH = 32K
}

ENTRY(reset_handler)

SECTIONS
{
    .vectors :
    {
        KEEP(*(.vectors))
    } > FLASH

    .text :
    {
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
    } > FLASH

    .rodata :
    {
        *(.rodata*)
    } > FLASH

    /* .data: load from flash, run from RAM */
    .data :
    {
        _data_start = .;
        *(.data*)
        _data_end = .;
    } > RAM AT > FLASH
    _data_load = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    } > RAM

    /* Top of RAM = initial stack pointer */
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}

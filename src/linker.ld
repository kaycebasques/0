/* Linker script for Raspberry Pi Pico 1 (RP2040 - ARM Cortex-M0+) */

MEMORY
{
    FLASH (rx) : ORIGIN = 0x10000000, LENGTH = 2M
    RAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 264K
}

ENTRY(Reset_Handler)

SECTIONS
{
    .boot2 :
    {
        KEEP(*(.boot2))
        . = 0x100;
    } > FLASH

    .text :
    {
        KEEP(*(.vectors))
        *(.text*)
        *(.rodata*)
    } > FLASH

    __stack_top = ORIGIN(RAM) + LENGTH(RAM);
}

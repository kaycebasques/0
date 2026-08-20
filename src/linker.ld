/* ============================================================================
 * LINKER SCRIPT FOR BARE-METAL RP2040 (ARM CORTEX-M0+)
 * ============================================================================
 *
 * In host C programming (e.g. Linux x86_64), the operating system loader maps
 * virtual memory sections automatically. In embedded bare-metal development,
 * there is no operating system. The linker script instructs GNU ld (or LLVM lld)
 * exactly where to place code, vectors, data, and stack in physical memory.
 */

MEMORY
{
    /* External QSPI Flash memory: Read (r) and Execute (x) starting at 0x10000000 */
    FLASH (rx) : ORIGIN = 0x10000000, LENGTH = 2M

    /* Internal SRAM: Read (r), Write (w), and Execute (x) starting at 0x20000000 */
    RAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 264K
}

/* Specify the initial execution entry point symbol in the ELF header */
ENTRY(Reset_Handler)

SECTIONS
{
    /* .boot2 Section: Must reside at the very beginning of Flash (0x10000000).
     * The RP2040 bootrom verifies the 256-byte checksummed boot stage 2 sector
     * before handing execution off to our code. */
    .boot2 :
    {
        KEEP(*(.boot2))
        . = 0x100; /* Force section size to exactly 256 bytes (0x100) */
    } > FLASH

    /* .text Section: Executable instructions and constant data, starting at 0x10000100. */
    .text :
    {
        /* KEEP ensures the ARM Cortex-M vector table is not stripped by --gc-sections */
        KEEP(*(.vectors))
        *(.text*)   /* Compiled machine instructions */
        *(.rodata*) /* Read-only constants, string literals */
    } > FLASH

    /* Calculate the initial stack top address.
     * ARM Cortex-M stacks use a "Full Descending" stack model: the stack pointer
     * starts at the highest SRAM address (0x20042000) and grows downwards towards 0x20000000. */
    __stack_top = ORIGIN(RAM) + LENGTH(RAM);
}

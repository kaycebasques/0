/* ============================================================================
 * LINKER SCRIPT FOR BARE-METAL RP2040 (ARM CORTEX-M0+)
 * ============================================================================
 *
 * In host C programming (e.g. Linux x86_64), the operating system loader maps
 * virtual memory sections automatically. In embedded bare-metal development,
 * there is no operating system. The linker script instructs GNU ld (or LLVM lld)
 * exactly where to place code, vectors, data, and stack in physical memory.
 *
 * [source: docs/rp2040.pdf "2.2 Address Map"]
 * [source: docs/dui0662b.pdf "2.1 Memory model"]
 * [source: examples/pico-sdk/src/rp2040/pico_platform/memmap_default.ld]
 */

MEMORY
{
    /* External QSPI Flash memory: Read (r) and Execute (x) starting at 0x10000000 (2MB total XIP window)
     * [source: docs/rp2040.pdf "2.2 Address Map"]
     * [source: examples/pico-sdk/src/rp2040/pico_platform/memmap_default.ld]
     */
    FLASH (rx) : ORIGIN = 0x10000000, LENGTH = 2M

    /* Internal SRAM: Read (r), Write (w), and Execute (x) starting at 0x20000000 (SRAM 0-5 total 264KB)
     * [source: docs/rp2040.pdf "2.2 Address Map"]
     * [source: examples/pico-sdk/src/rp2040/pico_platform/memmap_default.ld]
     */
    RAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 264K
}

/* Specify the initial execution entry point symbol in the ELF header */
ENTRY(Reset_Handler)

SECTIONS
{
    /* .boot2 Section: Must reside at the very beginning of Flash (0x10000000).
     * The RP2040 bootrom verifies the 256-byte checksummed boot stage 2 sector
     * before handing execution off to our code at 0x10000100.
     * [source: docs/rp2040.pdf "2.8.1.3 Flash Second Stage"]
     * [source: examples/pico-sdk/src/rp2040/pico_platform/memmap_default.ld]
     */
    .boot2 :
    {
        KEEP(*(.boot2))
        . = 0x100; /* Force section size to exactly 256 bytes (0x100) */
    } > FLASH

    /* .text Section: Executable instructions and constant data, starting at offset 0x10000100.
     * Vector table MUST be placed first at 0x10000100 so VTOR can point directly to it.
     * [source: docs/rp2040.pdf "2.8.1.3 Flash Second Stage"]
     * [source: docs/dui0662b.pdf "2.3.4 Vector table"]
     * [source: examples/pico-sdk/src/rp2040/pico_platform/memmap_default.ld]
     */
    .text :
    {
        /* KEEP ensures the ARM Cortex-M vector table is not stripped by --gc-sections */
        KEEP(*(.vectors))
        *(.text*)   /* Compiled machine instructions */
        *(.rodata*) /* Read-only constants, string literals */
    } > FLASH

    /* Calculate initial Main Stack Pointer (MSP) address.
     * ARM Cortex-M stacks use a "Full Descending" stack model: stack pointer
     * starts at the highest SRAM address (0x20000000 + 264K = 0x20042000) and grows downwards.
     * [source: docs/rp2040.pdf "2.2 Address Map"]
     * [source: docs/dui0662b.pdf "2.1.2 Stacks"]
     * [source: examples/pico-sdk/src/rp2040/pico_platform/memmap_default.ld]
     */
    __stack_top = ORIGIN(RAM) + LENGTH(RAM);
}


/**
 * @file main.c
 * @brief Bare-metal RP2040 (Raspberry Pi Pico) firmware implementation.
 *
 * This file provides a minimal, single-file C implementation for booting an
 * ARM Cortex-M0+ microcontroller (RP2040) from flash memory without relying
 * on any external C libraries, SDKs, or startup runtime routines.
 *
 * ============================================================================
 * EMBEDDED ARCHITECTURE OVERVIEW FOR SOFTWARE ENGINEERS
 * ============================================================================
 * Microcontrollers use Memory-Mapped I/O (MMIO). This means hardware
 * peripherals (GPIO pins, reset controllers, clocks, UARTs) are controlled by
 * reading and writing to specific 32-bit physical memory addresses.
 *
 * Address Space Map on RP2040:
 *   - 0x10000000 - 0x11000000: External SPI Flash (2MB - 16MB)
 *   - 0x20000000 - 0x20042000: Internal SRAM (264KB total across 6 banks)
 *   - 0x40000000 - 0x40070000: APB Peripherals (RESETS, IO_BANK0, PADS, etc.)
 *   - 0xd0000000 - 0xd000017c: SIO (Single-cycle I/O block for fast GPIO)
 *   - 0xe0000000 - 0xe000ed88: Private Peripheral Bus (ARM Cortex-M0+ SCB/VTOR)
 */

#include <stdint.h>

/**
 * MMIO Pointer Dereference Macro:
 * The `volatile` qualifier is essential in embedded C. It tells the compiler
 * that the value at this memory address can change outside the compiler's
 * knowledge (by hardware) and that writes have side effects. Without `volatile`,
 * GCC optimizations (like O2/O3) would optimize away register reads and writes.
 */
#define REG(addr) (*(volatile uint32_t *)(addr))

/* ----------------------------------------------------------------------------
 * RP2040 Peripheral Base Addresses
 * ---------------------------------------------------------------------------- */

/** RESETS: Controls reset state of peripherals on system power-up. */
#define RESETS_BASE     0x4000c000u

/** IO_BANK0: Configures pin multiplexing (which peripheral function connects to which pin). */
#define IO_BANK0_BASE   0x40014000u

/** PADS_BANK0: Configures electrical pad properties (drive strength, pulls, input enable). */
#define PADS_BANK0_BASE 0x4001c000u

/** SIO: Single-cycle I/O peripheral for direct, fast CPU pin manipulation. */
#define SIO_BASE        0xd0000000u

/** PPB: ARM Cortex-M0+ Internal Core Peripherals (VTOR, SysTick, NVIC). */
#define PPB_BASE        0xe0000000u

/** Onboard LED on standard Raspberry Pi Pico (RP2040) is connected to GPIO 25. */
#define LED_PIN 25

/* ----------------------------------------------------------------------------
 * RP2040 Boot Stage 2 Sector (.boot2)
 * ----------------------------------------------------------------------------
 * The RP2040 internal ROM bootloader executes upon power-up. It reads the first
 * 256 bytes of SPI flash at address 0x10000000.
 *
 * This 256-byte sector must configure the SPI flash controller speed and contain
 * a valid 32-bit CRC32-MPEG2 checksum in the last 4 bytes (offset 0xfc). If the
 * checksum is invalid, the ROM bootloader treats flash as unformatted and drops
 * into BOOTSEL (USB mass storage) mode.
 */
__attribute__((section(".boot2"), used))
const uint8_t boot2_raw[256] = {
    /* 16-byte Thumb assembly routine to jump to Vector Table at 0x10000100 */
    0x03, 0x48, 0x03, 0x49, 0x08, 0x60, 0x06, 0xc8,
    0x81, 0xf3, 0x08, 0x88, 0x10, 0x47, 0x00, 0xbf,
    0x00, 0x01, 0x00, 0x10, 0x08, 0xed, 0x00, 0xe0,
    /* 228 bytes zero padding */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 4-byte CRC32-MPEG2 checksum required by RP2040 bootloader */
    0xde, 0x6a, 0xa3, 0x9e
};

/* Defined in linker.ld: Top address of SRAM (0x20042000) */
extern uint32_t __stack_top;

void Reset_Handler(void);

/* ----------------------------------------------------------------------------
 * ARM Cortex-M Vector Table (.vectors)
 * ----------------------------------------------------------------------------
 * All ARM Cortex-M processors expect a vector table at address 0x00000000 (or
 * offset relocated by VTOR).
 *
 * Vector Table Schema:
 *   - Index 0: Initial Main Stack Pointer (MSP) value loaded into R13 hardware register
 *   - Index 1: Reset Handler function pointer (entry point after CPU reset)
 */
__attribute__((section(".vectors"), used))
void (* const vector_table[2])(void) = {
    (void (*)(void))&__stack_top, /* Index 0: Initial Stack Pointer */
    Reset_Handler,                /* Index 1: CPU Reset Entry Point */
};

/**
 * CPU Reset Handler Entry Point.
 *
 * When the microcontroller boots, the hardware reads vector_table[0] to initialize
 * the stack pointer, then jumps to vector_table[1] (Reset_Handler).
 */
void Reset_Handler(void) {
    /* 1. Initialize Main Stack Pointer (MSP) in CPU register R13 */
    __asm__ volatile ("msr msp, %0" : : "r" (&__stack_top));

    /* 2. Update Vector Table Offset Register (VTOR) in System Control Block (SCB) */
    REG(PPB_BASE + 0xed08) = (uint32_t)vector_table;

    /* 3. Release peripherals from hardware reset state (writing 0 clears all resets) */
    REG(RESETS_BASE + 0x00) = 0x00000000;

    /* 4. Configure GPIO 25 pad properties (0x56 = 8mA drive, Schmitt trigger, IE=1, OD=0) */
    REG(PADS_BANK0_BASE + 4u * LED_PIN + 4u) = 0x56;

    /* 5. Set GPIO 25 pin function to SIO (Function 5 = SIO block) */
    REG(IO_BANK0_BASE + 8u * LED_PIN + 4u) = 5;

    /* 6. Enable output direction and set pin HIGH using SIO registers */
    REG(SIO_BASE + 0x0038) = (1u << LED_PIN); /* SIO_GPIO_OE_SET (Output Enable) */
    REG(SIO_BASE + 0x0018) = (1u << LED_PIN); /* SIO_GPIO_OUT_SET (Set Pin High) */

    /* 7. Infinite loop: Embedded main routines must never exit */
    while (1) {}
}

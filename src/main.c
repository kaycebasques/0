/**
 * @file main.c
 * @brief Bare-Metal RP2040 Firmware Implementation & Architecture Reference.
 *
 * ============================================================================
 * EMBEDDED CONCEPTS FOR SOFTWARE ENGINEERS
 * ============================================================================
 *
 * 1. MEMORY-MAPPED I/O (MMIO) & ATOMIC ALIASES:
 *    Microcontrollers do not use system calls to talk to hardware. Peripherals
 *    are mapped into memory space starting at 0x40000000.
 *
 *    RP2040 features 4 atomic register access aliases for every peripheral:
 *      - Base + 0x0000 : Normal Read/Write
 *      - Base + 0x1000 : Atomic Bitwise XOR (toggles specified bits)
 *      - Base + 0x2000 : Atomic Bitwise SET (sets specified bits to 1)
 *      - Base + 0x3000 : Atomic Bitwise CLR (clears specified bits to 0)
 *
 *    Why Atomic Aliases Matter:
 *    Standard C read-modify-write (`reg |= mask`) requires 3 assembly instructions
 *    (LDR, ORR, STR). If an interrupt occurs between LDR and STR, another ISR
 *    might modify `reg`, causing data corruption (race condition). Writing to an
 *    atomic SET or CLR alias performs the operation in a single hardware cycle!
 *
 * 2. HARDWARE DIFFERENCES: RP2040 vs RP2350 SIO OFFSETS:
 *    - RP2040 (Pico 1) SIO Offsets:
 *        OUT_SET = 0x0014, OUT_CLR = 0x0018, OE_SET = 0x0024
 *    - RP2350 (Pico 2) SIO Offsets:
 *        OUT_SET = 0x0018, OUT_CLR = 0x0020, OE_SET = 0x0038
 *    (RP2350 extended SIO registers to support 48 GPIO pins instead of 30).
 */

#include <stdint.h>

/**
 * Memory-Mapped I/O Pointer Dereference Macro:
 * The `volatile` keyword tells GCC/Clang not to optimize away repeated reads/writes
 * or reorder memory operations, ensuring every access generates a physical bus cycle.
 */
#define REG(addr) (*(volatile uint32_t *)(addr))

/* ----------------------------------------------------------------------------
 * RP2040 Peripheral Base Addresses
 * ---------------------------------------------------------------------------- */
#define RESETS_BASE     0x4000c000u
#define IO_BANK0_BASE   0x40014000u
#define PADS_BANK0_BASE 0x4001c000u
#define SIO_BASE        0xd0000000u
#define PPB_BASE        0xe0000000u

/* ----------------------------------------------------------------------------
 * RP2040 Single-Cycle I/O (SIO) Peripheral Registers
 * ---------------------------------------------------------------------------- */
#define SIO_GPIO_OUT_SET REG(SIO_BASE + 0x0014) /* Output Set Alias (0x14) */
#define SIO_GPIO_OUT_CLR REG(SIO_BASE + 0x0018) /* Output Clear Alias (0x18) */
#define SIO_GPIO_OUT_XOR REG(SIO_BASE + 0x001c) /* Output Toggle Alias (0x1c) */
#define SIO_GPIO_OE_SET  REG(SIO_BASE + 0x0024) /* Output Enable Set Alias (0x24) */
#define SIO_GPIO_OE_CLR  REG(SIO_BASE + 0x0028) /* Output Enable Clear Alias (0x28) */
#define SIO_GPIO_OE_XOR  REG(SIO_BASE + 0x002c) /* Output Enable Toggle Alias (0x2c) */

/** Onboard LED on standard Raspberry Pi Pico (RP2040) is connected to GPIO 25. */
#define LED_PIN 25

/* Top address of SRAM defined in linker.ld (0x20042000) */
extern uint32_t __stack_top;

void Reset_Handler(void);
void Default_Handler(void);

/** Defensive handler trap for unhandled hardware exceptions. */
void Default_Handler(void) {
    while (1) {}
}

/* ----------------------------------------------------------------------------
 * ARM Cortex-M0+ Vector Table (.vectors)
 * ----------------------------------------------------------------------------
 * Placed at offset 0x10000100 in Flash. The ARM CPU hardware automatically:
 *   - Loads vector_table[0] into R13 (Main Stack Pointer) upon reset.
 *   - Loads vector_table[1] into R15 (Program Counter) to begin execution.
 */
__attribute__((section(".vectors"), used))
void (* const vector_table[48])(void) = {
    (void (*)(void))&__stack_top, /* Index 0: Stack Pointer Initial Value */
    Reset_Handler,                /* Index 1: CPU Reset Entry Point */
    Default_Handler,              /* Index 2: Non-Maskable Interrupt (NMI) */
    Default_Handler,              /* Index 3: HardFault Exception Handler */
    Default_Handler,              /* Index 4..15: ARM Reserved Exceptions */
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler,
    /* Index 16..47: RP2040 External Hardware IRQs (0 to 31) */
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler, Default_Handler,
};

/**
 * CPU Reset Handler Entry Point.
 */
void Reset_Handler(void) {
    /* 1. Explicitly initialize Main Stack Pointer (MSP / R13) */
    __asm__ volatile ("msr msp, %0" : : "r" (&__stack_top));

    /* 2. Set Vector Table Offset Register (VTOR) in SCB to 0x10000100 */
    REG(PPB_BASE + 0xed08) = (uint32_t)vector_table;

    /* 3. Release IO_BANK0 (bit 5) and PADS_BANK0 (bit 8) from hardware reset
     *    using the Atomic Clear Alias (0x4000c000 + 0x3000 = 0x4000f000). */
    REG(RESETS_BASE + 0x3000) = (1u << 5) | (1u << 8);

    /* 4. Configure GPIO 25 pad electrical properties:
     *    0x56 = 8mA Drive, Schmitt Trigger Enabled, Input Enable (IE=1), Output Disable (OD=0). */
    REG(PADS_BANK0_BASE + 4u * LED_PIN + 4u) = 0x56;

    /* 5. Set GPIO 25 pin function to SIO (Function 5 = Single-cycle I/O). */
    REG(IO_BANK0_BASE + 8u * LED_PIN + 4u) = 5;

    /* 6. Enable SIO GPIO 25 Output Direction (writing to RP2040 OE_SET alias 0x24). */
    SIO_GPIO_OE_SET = (1u << LED_PIN);

    /* 7. Drive GPIO 25 HIGH to turn LED ON solid (writing to RP2040 OUT_SET alias 0x14). */
    SIO_GPIO_OUT_SET = (1u << LED_PIN);

    /* 8. Main Infinite Loop */
    while (1) {}
}

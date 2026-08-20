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
 *
 * [source: docs/rp2040.pdf "2.1.2 Atomic Register Aliases"]
 * [source: examples/pico-sdk/src/rp2040/hardware_regs/include/hardware/address_mapped.h]
 */
#define REG(addr) (*(volatile uint32_t *)(addr))

/* ----------------------------------------------------------------------------
 * RP2040 Peripheral Base Addresses
 * ----------------------------------------------------------------------------
 * These physical memory addresses map directly to internal hardware peripheral
 * control blocks on the System Bus (AHB/APB).
 *
 * [source: docs/rp2040.pdf "2.2 Address Map"]
 * [source: examples/pico-sdk/src/rp2040/hardware_regs/include/hardware/regs/addressmap.h]
 * ---------------------------------------------------------------------------- */

// Subsystem Resets controller base address (0x4000c000)
// [source: docs/rp2040.pdf "2.2 Address Map"]
// [source: docs/rp2040.pdf "2.14 Subsystem Resets"]
// [source: examples/pico-sdk/src/rp2040/hardware_regs/include/hardware/regs/addressmap.h]
#define RESETS_BASE     0x4000c000u

// GPIO Bank 0 User control register base address (0x40014000)
// [source: docs/rp2040.pdf "2.2 Address Map"]
// [source: docs/rp2040.pdf "2.19.6.1 IO - User Bank"]
// [source: examples/pico-sdk/src/rp2040/hardware_regs/include/hardware/regs/addressmap.h]
#define IO_BANK0_BASE   0x40014000u

// GPIO Bank 0 Electrical Pad control register base address (0x4001c000)
// [source: docs/rp2040.pdf "2.2 Address Map"]
// [source: docs/rp2040.pdf "2.19.6.3 Pad Control - User Bank"]
// [source: examples/pico-sdk/src/rp2040/hardware_regs/include/hardware/regs/addressmap.h]
#define PADS_BANK0_BASE 0x4001c000u

// Single-Cycle I/O (SIO) peripheral base address (0xd0000000)
// [source: docs/rp2040.pdf "2.2 Address Map"]
// [source: docs/rp2040.pdf "2.3.1 SIO"]
// [source: examples/pico-sdk/src/rp2040/hardware_regs/include/hardware/regs/addressmap.h]
#define SIO_BASE        0xd0000000u

// Private Peripheral Bus (PPB) ARM Cortex-M0+ System Control Space base address (0xe0000000)
// [source: docs/rp2040.pdf "2.2 Address Map"]
// [source: docs/dui0662b.pdf "4.1 About the Cortex-M0+ System Control Block"]
// [source: examples/pico-sdk/src/rp2040/hardware_regs/include/hardware/regs/addressmap.h]
#define PPB_BASE        0xe0000000u

/* ----------------------------------------------------------------------------
 * RP2040 Single-Cycle I/O (SIO) Peripheral Registers
 * ----------------------------------------------------------------------------
 * RP2040 SIO provides atomic bit manipulation registers for GPIO outputs.
 * Note: RP2040 SIO offsets differ from RP2350 (Pico 2)!
 *   - RP2040: GPIO_OUT_SET = 0x0014, GPIO_OE_SET = 0x0024
 *   - RP2350: GPIO_OUT_SET = 0x0018, GPIO_OE_SET = 0x0038
 *
 * [source: docs/rp2040.pdf "2.3.1.7 Register List"]
 * [source: docs/rp2350.pdf "2.4 SIO"]
 * [source: examples/pico-sdk/src/rp2040/hardware_regs/include/hardware/regs/sio.h]
 * ---------------------------------------------------------------------------- */

// [source: docs/rp2040.pdf "2.3.1.7 Register List" (GPIO_OUT_SET offset 0x014)]
// [source: examples/pico-sdk/src/rp2040/hardware_regs/include/hardware/regs/sio.h]
#define SIO_GPIO_OUT_SET REG(SIO_BASE + 0x0014) /* Output Set Alias (0x14) */

// [source: docs/rp2040.pdf "2.3.1.7 Register List" (GPIO_OUT_CLR offset 0x018)]
// [source: examples/pico-sdk/src/rp2040/hardware_regs/include/hardware/regs/sio.h]
#define SIO_GPIO_OUT_CLR REG(SIO_BASE + 0x0018) /* Output Clear Alias (0x18) */

// [source: docs/rp2040.pdf "2.3.1.7 Register List" (GPIO_OUT_XOR offset 0x01c)]
// [source: examples/pico-sdk/src/rp2040/hardware_regs/include/hardware/regs/sio.h]
#define SIO_GPIO_OUT_XOR REG(SIO_BASE + 0x001c) /* Output Toggle Alias (0x1c) */

// [source: docs/rp2040.pdf "2.3.1.7 Register List" (GPIO_OE_SET offset 0x024)]
// [source: examples/pico-sdk/src/rp2040/hardware_regs/include/hardware/regs/sio.h]
#define SIO_GPIO_OE_SET  REG(SIO_BASE + 0x0024) /* Output Enable Set Alias (0x24) */

// [source: docs/rp2040.pdf "2.3.1.7 Register List" (GPIO_OE_CLR offset 0x028)]
// [source: examples/pico-sdk/src/rp2040/hardware_regs/include/hardware/regs/sio.h]
#define SIO_GPIO_OE_CLR  REG(SIO_BASE + 0x0028) /* Output Enable Clear Alias (0x28) */

// [source: docs/rp2040.pdf "2.3.1.7 Register List" (GPIO_OE_XOR offset 0x02c)]
// [source: examples/pico-sdk/src/rp2040/hardware_regs/include/hardware/regs/sio.h]
#define SIO_GPIO_OE_XOR  REG(SIO_BASE + 0x002c) /* Output Enable Toggle Alias (0x2c) */

/**
 * Onboard LED on standard Raspberry Pi Pico (RP2040) is connected to GPIO 25.
 *
 * [source: docs/pico1.pdf "2.2 Board Specifications"]
 * [source: examples/pico-examples/blink/blink.c]
 */
#define LED_PIN 25

/* Top address of SRAM defined in linker.ld (0x20042000 = ORIGIN(RAM) + LENGTH(RAM))
 * [source: docs/rp2040.pdf "2.2 Address Map"]
 * [source: docs/dui0662b.pdf "2.1.2 Stacks"]
 * [source: examples/pico-sdk/src/rp2040/pico_platform/memmap_default.ld]
 */
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
 * Placed at offset 0x10000100 in Flash (immediately after the 256-byte .boot2 sector).
 * The ARM CPU hardware automatically reads the vector table on startup:
 *   - Entry 0 (offset 0x00): Initial Main Stack Pointer (MSP / R13) value.
 *   - Entry 1 (offset 0x04): Initial Program Counter (PC / R15 / Reset_Handler) value.
 *
 * [source: docs/dui0662b.pdf "2.3.4 Vector table"]
 * [source: docs/rp2040.pdf "2.3.2 Interrupts"]
 * [source: examples/pico-sdk/src/rp2_common/pico_crt0/crt0.S]
 * ---------------------------------------------------------------------------- */
__attribute__((section(".vectors"), used))
void (* const vector_table[48])(void) = {
    (void (*)(void))&__stack_top, /* Index 0: Stack Pointer Initial Value (0x20042000) */
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
 * Execution begins here immediately after vector table readout by ARM CPU core.
 */
void Reset_Handler(void) {
    /* 1. Explicitly initialize Main Stack Pointer (MSP / R13)
     * [source: docs/dui0662b.pdf "3.1.4 MSR"]
     */
    __asm__ volatile ("msr msp, %0" : : "r" (&__stack_top));

    /* 2. Set Vector Table Offset Register (VTOR) in System Control Block (SCB)
     * PPB_BASE (0xe0000000) + VTOR Offset (0xed08) = 0xe000ed08.
     * Tells the CPU hardware that the active exception vector table is located at 0x10000100 in Flash.
     * [source: docs/dui0662b.pdf "4.3.5 Vector Table Offset Register"]
     * [source: docs/rp2040.pdf "2.4.4 Cortex-M0+ Core Registers"]
     * [source: examples/pico-sdk/src/rp2_common/pico_crt0/crt0.S]
     */
    REG(PPB_BASE + 0xed08) = (uint32_t)vector_table;

    /* 3. Release IO_BANK0 (bit 5) and PADS_BANK0 (bit 8) from hardware reset
     *    using the Atomic Clear Alias (RESETS_BASE 0x4000c000 + Atomic CLR Offset 0x3000 = 0x4000f000).
     *    Writing 1s to the clear alias clears the reset control bits, enabling the peripherals.
     * [source: docs/rp2040.pdf "2.1.2 Atomic Register Aliases"]
     * [source: docs/rp2040.pdf "2.14.3 List of Registers" (RESET Register: bit 5 = io_bank0, bit 8 = pads_bank0)]
     * [source: examples/pico-sdk/src/rp2040/hardware_regs/include/hardware/regs/resets.h]
     */
    REG(RESETS_BASE + 0x3000) = (1u << 5) | (1u << 8);

    /* 4. Configure GPIO 25 pad electrical properties:
     *    Register offset: PADS_BANK0_BASE (0x4001c000) + 4u (header offset) + (4u * LED_PIN (25)) = 0x4001c068.
     *    Value 0x56 (0b01010110):
     *      - Bit 1 (OD = 0): Output Disable cleared (output driver active)
     *      - Bit 2 (IE = 1): Input Enable set
     *      - Bits 4..5 (DRIVE = 2): 8mA drive strength
     *      - Bit 6 (PDE = 1): Pull-down resistor enabled
     * [source: docs/rp2040.pdf "2.19.6.3 Pad Control - User Bank" (PAD_GPIO25 register)]
     * [source: examples/pico-sdk/src/rp2040/hardware_regs/include/hardware/regs/pads_bank0.h]
     */
    REG(PADS_BANK0_BASE + 4u * LED_PIN + 4u) = 0x56;

    /* 5. Set GPIO 25 pin function to SIO (Function 5 = Single-cycle I/O).
     *    Register offset: IO_BANK0_BASE (0x40014000) + (8u * LED_PIN (25)) + 4u = 0x400140cc (GPIO25_CTRL).
     *    Value 5: Function 5 maps GPIO 25 pin multiplexer to the SIO block.
     * [source: docs/rp2040.pdf "2.19.2 Function Select" (Table 288: GPIO25_CTRL FUNCSEL=5 SIO)]
     * [source: docs/rp2040.pdf "2.19.6.1 IO - User Bank" (GPIO25_CTRL register)]
     * [source: examples/pico-sdk/src/rp2040/hardware_regs/include/hardware/regs/io_bank0.h]
     */
    REG(IO_BANK0_BASE + 8u * LED_PIN + 4u) = 5;

    /* 6. Enable SIO GPIO 25 Output Direction (writing to RP2040 OE_SET alias offset 0x24).
     *    Writing (1 << 25) atomically sets bit 25 in the GPIO Output Enable register without affecting other pins.
     * [source: docs/rp2040.pdf "2.3.1.7 Register List" (GPIO_OE_SET offset 0x024)]
     * [source: examples/pico-sdk/src/rp2040/hardware_regs/include/hardware/regs/sio.h]
     */
    SIO_GPIO_OE_SET = (1u << LED_PIN);

    /* 7. Drive GPIO 25 HIGH to turn LED ON solid (writing to RP2040 OUT_SET alias offset 0x14).
     *    Writing (1 << 25) atomically sets bit 25 in the GPIO Output register, pulling the pin to 3.3V.
     * [source: docs/rp2040.pdf "2.3.1.7 Register List" (GPIO_OUT_SET offset 0x014)]
     * [source: examples/pico-sdk/src/rp2040/hardware_regs/include/hardware/regs/sio.h]
     */
    SIO_GPIO_OUT_SET = (1u << LED_PIN);

    /* 8. Main Infinite Loop */
    while (1) {}
}


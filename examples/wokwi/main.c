/**
 * @file main.c
 * @brief Pico SDK High-Level Blinky Application.
 *
 * This example demonstrates microcontroller programming using official vendor SDK
 * abstractions (`pico/stdlib.h`) rather than bare-metal register manipulation.
 *
 * SDK vs. Bare-Metal Comparison for Engineers:
 *   - Bare-Metal: Direct manipulation of MMIO memory addresses, manual clock setup,
 *     linker script alignment, and custom vector tables.
 *   - SDK Abstraction: `pico_stdlib` automatically handles system clock initialization
 *     (XOSC 12MHz -> PLL 125MHz), peripheral reset release, pad configuration, and
 *     hardware timer alarm pools for timing functions like `sleep_ms()`.
 */

#include "pico/stdlib.h"

int main(void) {
    /**
     * Standard onboard LED pin definition for Raspberry Pi Pico (RP2040).
     */
    const uint LED_PIN = 25;

    /**
     * gpio_init(pin):
     * Releases GPIO peripheral from hardware reset state, sets pad drive strength,
     * enables input/output drivers, and assigns the pin function multiplexer to SIO.
     */
    gpio_init(LED_PIN);

    /**
     * gpio_set_dir(pin, GPIO_OUT):
     * Configures SIO direction register (SIO_GPIO_OE_SET) for output mode.
     */
    gpio_set_dir(LED_PIN, GPIO_OUT);

    /**
     * Main Execution Loop:
     * Toggles GPIO 25 High (3.3V) and Low (0V) with 250ms delays driven by
     * the RP2040 hardware timer alarm pool.
     */
    while (true) {
        gpio_put(LED_PIN, 1); /* Drive Pin High (Turn LED ON) */
        sleep_ms(250);        /* Pause execution using hardware timer interrupt */
        gpio_put(LED_PIN, 0); /* Drive Pin Low (Turn LED OFF) */
        sleep_ms(250);        /* Pause execution */
    }

    return 0;
}

/**
 * @file main.c
 * @brief Pico SDK High-Level Application (Turn Onboard LED Solid ON).
 *
 * This example demonstrates microcontroller programming using official vendor SDK
 * abstractions (`pico/stdlib.h`) rather than bare-metal register manipulation.
 */

#include "pico/stdlib.h"

int main(void) {
    /** Standard onboard LED pin definition for Raspberry Pi Pico (RP2040). */
    const uint LED_PIN = 25;

    /** Initialize GPIO 25 pad and peripheral function. */
    gpio_init(LED_PIN);

    /** Configure GPIO 25 as Output. */
    gpio_set_dir(LED_PIN, GPIO_OUT);

    /** Drive Pin High (Turn LED ON solid). */
    gpio_put(LED_PIN, 1);

    /** Infinite loop (stay ON solid). */
    while (true) {}

    return 0;
}

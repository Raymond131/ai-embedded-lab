/*
 * blinky_1hz — Stage 0: 1 Hz LED blink on GPIO2 (onboard LED, ESP32-WROOM-32D)
 *
 * Toggles GPIO2 every 500 ms — one full on-off cycle per second (1 Hz).
 * Prints AEL_BLINKY tick=N state=on|off for each toggle so AEL can verify
 * the firmware is alive over UART0 (CP210X console).
 *
 * After 20 toggles (10 s of blinking) prints AEL_BLINKY_DONE, then continues
 * blinking forever so the LED remains visible after the AEL run finishes.
 *
 * No jumpers. No external wiring. Onboard LED on GPIO2.
 * Flash: idf.py -p /dev/ttyUSB0 build flash monitor
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define LED_GPIO       GPIO_NUM_2
#define HALF_PERIOD_MS 500    /* 500 ms on + 500 ms off = 1 Hz */
#define VERIFY_TICKS   20     /* 10 s of blinking before AEL_BLINKY_DONE */

void app_main(void)
{
    printf("AEL_BLINKY BOOT gpio=GPIO%d freq_hz=1\n", LED_GPIO);
    fflush(stdout);

    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    int level = 0;
    for (int i = 1; i <= VERIFY_TICKS; i++) {
        level ^= 1;
        gpio_set_level(LED_GPIO, level);
        printf("AEL_BLINKY tick=%d state=%s\n", i, level ? "on" : "off");
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(HALF_PERIOD_MS));
    }

    printf("AEL_BLINKY_DONE ticks=%d freq_hz=1\n", VERIFY_TICKS);
    fflush(stdout);

    /* Keep blinking forever so the LED stays visible after the test passes. */
    for (;;) {
        level ^= 1;
        gpio_set_level(LED_GPIO, level);
        vTaskDelay(pdMS_TO_TICKS(HALF_PERIOD_MS));
    }
}

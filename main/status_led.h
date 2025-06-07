// status_led.h
#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint16_t hue;
    uint16_t saturation;
    uint16_t value;
} led_hsv;

/**
 * @brief Initialize and configure on-board status LED.
 */
void configure_led(void);

/**
 * @brief Turns the status LED on using the color info @p hsv_info.
 * 
 * @param hsv_info The HSV color info to use for the LED.
 */
void status_led_on(const led_hsv* hsv_info);

/**
 * @brief Turns the status LED off.
 */
void status_led_off(void);

#ifdef __cplusplus
}
#endif

#endif // STATUS_LED_H
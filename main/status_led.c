#include "esp_err.h"
#include "esp_log.h"
#include "led_strip.h"
#include "constants.h"
#include "status_led.h"

static led_strip_handle_t led_strip;

void configure_led(void)
{
    ESP_LOGI(TAG, "Configured to blink addressable LED!");
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = 1, // at least one LED on board
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
}

void status_led_on(const led_hsv* hsv_info)
{
    led_strip_set_pixel_hsv(led_strip, 0, hsv_info->hue, hsv_info->saturation, hsv_info->value);
    led_strip_refresh(led_strip);
}

void status_led_off(void)
{
    led_strip_clear(led_strip);
}

#include "esp_bit_defs.h"
#include "sdkconfig.h"
#include "constants.h"

const uint8_t     WIFI_MAX_RETRY         = 5;
const uint8_t     CORE_0                 = 0;
const uint8_t     CORE_1                 = 1;
const uint8_t     QUEUE_LENGTH           = 1;
const uint8_t     TASK_PRIORITY          = 1;
const uint8_t     UDP_MAX_ATTEMPTS       = 3;
const uint8_t     UDP_TIMEOUT            = 5;
const uint32_t    WIFI_CONNECTED_BIT     = BIT0;
const uint32_t    WIFI_FAIL_BIT          = BIT1;
const uint32_t    BLINK_GPIO             = CONFIG_BLINK_GPIO;
const led_hsv     COLOR_INFO_READ_SENSOR = { .hue = 300, .saturation = 255, .value = 20 };
const char* const TAG                    = "Temp/Humidity Sensor";


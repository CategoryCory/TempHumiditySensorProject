// constants.h
#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdint.h>
#include "status_led.h"

extern const uint8_t     CORE_0;
extern const uint8_t     CORE_1;
extern const uint8_t     QUEUE_LENGTH;
extern const uint8_t     TASK_PRIORITY;
extern const uint8_t     UDP_MAX_ATTEMPTS;
extern const uint8_t     UDP_TIMEOUT;
extern const uint8_t     WIFI_MAX_RETRY;
extern const uint32_t    WIFI_CONNECTED_BIT;
extern const uint32_t    WIFI_FAIL_BIT;
extern const uint32_t    BLINK_GPIO;
extern const led_hsv     COLOR_INFO_READ_SENSOR;
extern const char* const TAG;


#endif // CONSTANTS_H

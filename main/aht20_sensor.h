#ifndef AHT20_SENSOR_H
#define AHT20_SENSOR_H

#include <stdint.h>
#include "aht.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Task to read data from the AHT20 sensor.
 *
 * This task continuously reads temperature and humidity data from the AHT20 sensor
 * and sends the data to a message queue.
 *
 * @param pvParameters Pointer to task parameters.
 */
void read_aht20(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif // AHT20_SENSOR_H
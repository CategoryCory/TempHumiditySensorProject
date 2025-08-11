#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "aht20_sensor.h"
#include "config.h"
#include "constants.h"

void read_aht20(void *pvParameters)
{
    aht20_data recorded_data = {0};
    QueueHandle_t msg_queue = (QueueHandle_t) pvParameters;

    while (1) {
        // Read AHT20
        if (aht20_read_measures(&recorded_data) == 0)
        {
            if (xQueueSend(msg_queue, (void *) &recorded_data, 10) != pdTRUE)
            {
                ESP_LOGE(TAG, "Unable to add measurement to queue.");
            }
        }
        else
        {
            ESP_LOGE(TAG, "Unable to acquire reading from AHT20.");
        }

        // Wait before reading AHT20 again
        vTaskDelay((1000 * READ_SENSOR_SECONDS) / portTICK_PERIOD_MS);
    }
}
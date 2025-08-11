#include <socket.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "aht.h"
#include "aht20_sensor.h"
#include "cbor.h"
#include "config.h"
#include "constants.h"
#include "data_server.h"
#include "status_led.h"
#include "time_sync.h"
#include "wifi_manager.h"

void app_main(void)
{
    QueueHandle_t msg_queue;

    // Configure NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize queue
    msg_queue = xQueueCreate(QUEUE_LENGTH, sizeof(aht20_data));

    // Initialize services
    configure_led();
    aht20_i2c_setup();
    ESP_ERROR_CHECK(wifi_manager_start());
    ESP_ERROR_CHECK(esp_netif_init());

    // Wait to ensure services are connected
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // Set date and time
    sync_time();

    xTaskCreatePinnedToCore(read_aht20, 
                            "read_aht20", 
                            5000, 
                            (void *) msg_queue, 
                            1, 
                            NULL, 
                            CORE_0
                        );

    xTaskCreatePinnedToCore(send_data_to_server, 
                            "send_data_to_server", 
                            5000, 
                            (void *) msg_queue, 
                            1,
                            NULL,
                            CORE_1
                        );

    while (1) 
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

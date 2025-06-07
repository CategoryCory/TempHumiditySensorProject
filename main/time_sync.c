#include <stdlib.h>
#include <time.h>
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "portmacro.h"
#include "config.h"
#include "constants.h"
#include "time_sync.h"

static void time_sync_notification_db([[maybe_unused]] struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronization event");
}

void sync_time(void)
{
    ESP_LOGI(TAG, "Initializing SNTP and setting system time...");
    esp_sntp_config_t sntp_config = ESP_NETIF_SNTP_DEFAULT_CONFIG(SNTP_SERVER);
    sntp_config.sync_cb = time_sync_notification_db;
    sntp_config.smooth_sync = true;
    esp_netif_sntp_init(&sntp_config);

    // Wait for time to be set
    int retry = 0;
    const int retry_count = 15;

    while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && ++retry < retry_count)
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
    }
    
    esp_netif_sntp_deinit();

    // Set time zone information
    setenv("TZ", TIMEZONE, 1);
    tzset();
}

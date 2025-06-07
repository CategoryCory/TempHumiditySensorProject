#include <socket.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "aht.h"
#include "cbor.h"
#include "config.h"
#include "constants.h"
#include "status_led.h"
#include "time_sync.h"
#include "wifi_manager.h"

static QueueHandle_t msg_queue;
static int wifi_connect_retries;

void read_aht20(void *pvParameters)
{
    aht20_data recorded_data = {0};

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

void send_data_to_server(void *pvParameter)
{
    aht20_data recorded_data;
    CborEncoder encoder;
    CborEncoder map_encoder;
    uint8_t cbor_buffer[MAX_CBOR_BUFFER_SIZE];
    char ack_buffer[16];
    size_t encoded_size;

    int socketfd;
    struct sockaddr_in server_addr;
    struct sockaddr_in local_addr;
    struct timeval tv;

    int udp_attempts;
    bool udp_sent;

    time_t now;

    tv.tv_sec = UDP_TIMEOUT;
    tv.tv_usec = 0;

    // Create UDP socket
    if ((socketfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        ESP_LOGE(TAG, "Unable to establish socket connection.");
        exit(EXIT_FAILURE);
    }

    // Set socket timeout
    setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv, sizeof(tv));

    // Zero out structs
    memset(&server_addr, 0, sizeof(server_addr));
    memset(&local_addr, 0, sizeof(local_addr));

    // Configure server IP information
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(UDP_SERVER_IP);
    server_addr.sin_port = htons(UDP_SERVER_PORT);

    // Configure client (local) IP information
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(9999);

    bind(socketfd, (struct sockaddr *) &local_addr, sizeof(local_addr));

    while (1)
    {
        if (xQueueReceive(msg_queue, (void *) &recorded_data, 0) == pdTRUE)
        {
            // Turn LED on
            // led_strip_set_pixel_hsv(led_strip, 0, 300, 255, 20);
            // led_strip_refresh(led_strip);
            status_led_on(&COLOR_INFO_READ_SENSOR);
            vTaskDelay(100 / portTICK_PERIOD_MS);

            // Clear buffer for CBOR object
            memset(cbor_buffer, 0, sizeof(cbor_buffer));
            memset(ack_buffer, 0, sizeof(ack_buffer));

            // Get time for current recording
            now = time(NULL);

            // Initialize CBOR encoders
            cbor_encoder_init(&encoder, cbor_buffer, sizeof(cbor_buffer), 0);
            cbor_encoder_create_map(&encoder, &map_encoder, 3);

            // Create map -- temp_c:float
            cbor_encode_text_stringz(&map_encoder, "temp_c");
            cbor_encode_float(&map_encoder, recorded_data.temperature_celsius);  

            // Create map -- hmd:float
            cbor_encode_text_stringz(&map_encoder, "hmd");
            cbor_encode_float(&map_encoder, recorded_data.relative_humidity);

            // Create map -- time:uint64_t
            cbor_encode_text_stringz(&map_encoder, "time");
            cbor_encode_uint(&map_encoder, now);

            // Close CBOR container
            cbor_encoder_close_container(&encoder, &map_encoder);

            // Get buffer size
            encoded_size = cbor_encoder_get_buffer_size(&encoder, cbor_buffer);

            udp_attempts = 0;
            udp_sent = false;

            if (connect(socketfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) >= 0)
            {
                while (!udp_sent && udp_attempts < UDP_MAX_ATTEMPTS)
                {
                    ESP_LOGI(TAG, "Sending message...");
                    [[maybe_unused]] size_t bytes_sent = sendto(socketfd, cbor_buffer, encoded_size, 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
                    udp_attempts++;

                    ssize_t s_bytes_received = recvfrom(socketfd, ack_buffer, sizeof(cbor_buffer), 0, NULL, NULL);
                    if (s_bytes_received > 0)
                    {
                        ack_buffer[s_bytes_received] = '\0';
                        if (strcmp(ack_buffer, "ACK") == 0)
                        {
                            udp_sent = true;
                            ESP_LOGI(TAG, "ACK received. Data sent successfully.");
                        }
                    }
                    else
                    {
                        ESP_LOGI(TAG, "No ACK received. Resending data...");
                    }
                }

                if (!udp_sent)
                {
                    ESP_LOGI(TAG, "Failed to send data after %d attempts.", udp_attempts);
                }
            }
            else
            {
                ESP_LOGI(TAG, "UDP connection failed.");
            }

            // Turn LED off
            // led_strip_clear(led_strip);
            status_led_off();
        }

        vTaskDelay((1000 * SEND_DATA_SECONDS) / portTICK_PERIOD_MS);
    }

    close(socketfd);
}

void app_main(void)
{
    wifi_connect_retries = 0;

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
                            NULL, 
                            1, 
                            NULL, 
                            CORE_0
                        );

    xTaskCreatePinnedToCore(send_data_to_server, 
                            "send_data_to_server", 
                            5000, 
                            NULL, 
                            1, 
                            NULL,
                            CORE_1
                        );

    while (1) 
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

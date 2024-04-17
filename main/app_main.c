#include <socket.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif_sntp.h"
#include "nvs_flash.h"
#include "led_strip.h"
#include "aht.h"
#include "cbor.h"
#include "config.h"

#define BLINK_GPIO              CONFIG_BLINK_GPIO
#define TASK_PRIORITY           1
#define CORE_0                  0
#define CORE_1                  1
#define WIFI_MAX_RETRY          5
#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT           BIT1
#define MAX_CBOR_BUFFER_SIZE    128
#define UDP_MAX_ATTEMPTS        3
#define UDP_TIMEOUT             5
#define QUEUE_LENGTH            1

static const char *TAG = "Temp/Humidity Sensor";

static EventGroupHandle_t wifi_event_group;
static led_strip_handle_t led_strip;
static aht20_data shared_recorded_data;

static SemaphoreHandle_t data_available_semaphore;

static int wifi_connect_retries;

static void configure_led(void)
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

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (wifi_connect_retries < WIFI_MAX_RETRY)
        {
            esp_wifi_connect();
            wifi_connect_retries++;
            ESP_LOGI(TAG, "WiFi connection retry: %d", wifi_connect_retries);
        }
        else
        {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "Failed to connect to WiFi.");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "WiFi connected with IP address " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connect_retries = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            .sae_h2e_identifier = ""
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta complete.");

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "Connected to WiFi network %s", WIFI_SSID);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to WiFi network %s", WIFI_SSID);
    }
    else 
    {
        ESP_LOGE(TAG, "Unexpected error");
    }
    
}

static void time_sync_notification_db(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronization event");
}

static void sync_time(void)
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

void read_aht20(void *pvParameters)
{
    while (1) {
        // Read AHT20
        if (aht20_read_measures(&shared_recorded_data) == 0)
        {
            xSemaphoreGive(data_available_semaphore);
        }
        else
        {
            ESP_LOGE(TAG, "Unable to acquire reading from AHT20.");
        }

        // Wait before reading AHT20 again
        // TODO: Define time to delay as a #define constant
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void send_data_to_server(void *pvParameter)
{
    aht20_data recorded_data;
    CborEncoder encoder;
    CborEncoder map_encoder;
    uint8_t cbor_buffer[MAX_CBOR_BUFFER_SIZE];
    char ack_buffer[16];    // TODO: Should this be a #define constant?
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

    // Set UDP connection as blocking
    // TODO: Is this necessary?
    // int flags = fcntl(socketfd, F_GETFL, 0);
    // fcntl(socketfd, F_SETFL, flags & ~O_NONBLOCK);

    // Configure client (local) IP information
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(9999);

    bind(socketfd, (struct sockaddr *) &local_addr, sizeof(local_addr));

    while (1)
    {
        if (xSemaphoreTake(data_available_semaphore, 0) == pdPASS)
        {
            recorded_data = shared_recorded_data;

            // Turn LED on
            led_strip_set_pixel_hsv(led_strip, 0, 120, 255, 32);
            led_strip_refresh(led_strip);
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
                    size_t bytes_sent = sendto(socketfd, cbor_buffer, encoded_size, 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
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
            led_strip_clear(led_strip);
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
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

    // Create semaphore
    data_available_semaphore = xSemaphoreCreateBinary();

    // TODO: Make sure data_available_semaphore isn't NULL

    // Initialize services
    configure_led();
    aht20_i2c_setup();
    wifi_init_sta();
    ESP_ERROR_CHECK(esp_netif_init());

    // Wait to ensure services are connected
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // Set date and time
    sync_time();

    // TODO: Is this delay necessary?
    vTaskDelay(250 / portTICK_PERIOD_MS);

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

#include <socket.h>

#include "aht.h"
#include "cbor.h"
#include "config.h"
#include "constants.h"
#include "esp_log.h"

#include "data_server.h"

void send_data_to_server(void *pvParameters)
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

    QueueHandle_t msg_queue = (QueueHandle_t) pvParameters;

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
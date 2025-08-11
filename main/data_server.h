#ifndef DATA_SERVER_H
#define DATA_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Send data to the server.
 *
 * This function is responsible for sending sensor data to the remote server
 * using the UDP communication protocol. Data is sent in CBOR encoding.
 *
 * @param pvParameters Pointer to the parameters for the task.
 */
void send_data_to_server(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif // DATA_SERVER_H
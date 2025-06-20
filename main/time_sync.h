#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Syncs the internal time with an external time server.
 * 
 * This function initiates a synchronization with a configured time server.
 * It blocks until the time has been set or a number of failed attempts occurs.
 * 
 * @note This function requires network connectivity and should only be called
 * after the system has an active internet connection.
 * 
 * @warning If called before Wi-Fi is connected, this function may fail silently.
 */
void sync_time(void);

#ifdef __cplusplus
}
#endif

#endif // TIME_SYNC_H
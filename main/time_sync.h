// time_sync.h
#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Syncs the internal time with an external time server.
 */
void sync_time(void);

#ifdef __cplusplus
}
#endif

#endif // TIME_SYNC_H
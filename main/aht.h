#ifndef AHT_20
#define AHT_20

#include "freertos/FreeRTOS.h"
#include "driver/i2c.h"

// TODO: Use this header instead
// #include "driver/i2c_master.h"

#include "driver/gpio.h"
#include "esp_bit_defs.h"
#include "esp_check.h"
#include "esp_types.h"

#define AHT20_ADDR          0x38
#define AHT20_START_MEAS    0XAC
#define I2C_MASTER_PORT     I2C_NUM_0
#define I2C_SDA             42
#define I2C_SCL             41

#define AT581X_STATUS_CMP_INT               (2)
#define AT581X_STATUS_Calibration_Enable    (3)
#define AT581X_STATUS_CRC_FLAG              (4)
#define AT581X_STATUS_MODE_STATUS           (5)
#define AT581X_STATUS_BUSY_INDICATION       (7)

typedef void *aht20_dev_handle_t;

typedef struct {
    i2c_port_t  i2c_port;
    uint8_t     i2c_addr;
} aht20_dev_t;

typedef struct {
    float temperature_celsius;
    float relative_humidity;
} aht20_data;

void aht20_i2c_setup();
void check_calibration();
esp_err_t aht20_read_measures(aht20_data *data_out);

#endif
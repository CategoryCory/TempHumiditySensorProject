#include "aht.h"

const static char *TAG = "AHT20";

static i2c_config_t aht20_i2c_conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = I2C_SDA,
    .scl_io_num = I2C_SCL,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 400000
};

void aht20_i2c_setup() {
    i2c_param_config(I2C_MASTER_PORT, &aht20_i2c_conf);
    i2c_driver_install(I2C_MASTER_PORT, aht20_i2c_conf.mode, 0, 0, 0);
}

static esp_err_t aht20_write_reg(aht20_dev_handle_t dev, uint8_t reg_addr, uint8_t *data, uint8_t len)
{
    esp_err_t ret;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    ret = i2c_master_start(cmd);
    assert(ESP_OK == ret);
    ret = i2c_master_write_byte(cmd, (AHT20_ADDR << 1) | I2C_MASTER_WRITE, true);
    assert(ESP_OK == ret);
    ret = i2c_master_write_byte(cmd, AHT20_START_MEAS, true);
    assert(ESP_OK == ret);
    if (len) {
        ret = i2c_master_write(cmd, data, len, true);
        assert(ESP_OK == ret);
    }
    ret = i2c_master_stop(cmd);
    assert(ESP_OK == ret);
    ret = i2c_master_cmd_begin(I2C_MASTER_PORT, cmd, 5000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

static esp_err_t aht20_read_reg(aht20_dev_handle_t dev, uint8_t *data, size_t len)
{
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    ret = i2c_master_start(cmd);
    assert(ESP_OK == ret);
    ret = i2c_master_write_byte(cmd, (AHT20_ADDR << 1) | I2C_MASTER_READ, true);
    assert(ESP_OK == ret);
    ret = i2c_master_read(cmd, data, len, I2C_MASTER_LAST_NACK);
    assert(ESP_OK == ret);
    ret = i2c_master_stop(cmd);
    assert(ESP_OK == ret);
    ret = i2c_master_cmd_begin(I2C_MASTER_PORT, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

static uint8_t aht20_calc_crc(uint8_t *data, uint8_t len)
{
    uint8_t i;
    uint8_t byte;
    uint8_t crc = 0xFF;

    for (byte = 0; byte < len; byte++) {
        crc ^= data[byte];
        for (i = 8; i > 0; --i) {
            if ((crc & 0x80) != 0) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = crc << 1;
            }
        }
    }

    return crc;
}

esp_err_t aht20_read_measures(aht20_data *data_out) {
    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();

    uint8_t status;
    uint8_t buf[7];
    uint32_t raw_humidity;
    uint32_t raw_temperature;

    ESP_RETURN_ON_FALSE(cmd_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid AHT20 device handle pointer");

    buf[0] = 0x33;
    buf[1] = 0x00;
    ESP_RETURN_ON_ERROR(aht20_write_reg(cmd_handle, AHT20_START_MEAS, buf, 2), TAG, "I2C read/write error");

    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_RETURN_ON_ERROR(aht20_read_reg(cmd_handle, &status, 1), TAG, "I2C read/write error");

    if ((status & BIT(AT581X_STATUS_Calibration_Enable)) &&
            (status & BIT(AT581X_STATUS_CRC_FLAG)) &&
            ((status & BIT(AT581X_STATUS_BUSY_INDICATION)) == 0)) {
        ESP_RETURN_ON_ERROR(aht20_read_reg(cmd_handle, buf, 7), TAG, "I2C read/write error");
        ESP_RETURN_ON_ERROR((aht20_calc_crc(buf, 6) != buf[6]), TAG, "Error calculating AHT20 CRC");

        raw_humidity = buf[1];
        raw_humidity = raw_humidity << 8;
        raw_humidity += buf[2];
        raw_humidity = raw_humidity << 8;
        raw_humidity += buf[3];
        raw_humidity = raw_humidity >> 4;
        data_out->relative_humidity = (float)raw_humidity * 0.000095367;

        raw_temperature = buf[3] & 0x0F;
        raw_temperature = raw_temperature << 8;
        raw_temperature += buf[4];
        raw_temperature = raw_temperature << 8;
        raw_temperature += buf[5];
        data_out->temperature_celsius = (float)raw_temperature * 0.000190735 - 50;
        return ESP_OK;
    } else {
        ESP_LOGI(TAG, "data is not ready");
        return ESP_ERR_NOT_FINISHED;
    }
}
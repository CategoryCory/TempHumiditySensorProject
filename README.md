# ESP32 Wi-Fi Temperature and Humidity Sensor

_A lightweight environmental sensor using ESP-IDF, UDP, and the Adafruit AHT20_

This project is a compact, Wi-Fi-connected temperature and humidity sensor using an ESP32-S3-DevKitC-1 and an Adafruit AHT20 sensor. It periodically reads environmental data and transmits it via UDP to a remote server, using a basic retry-and-acknowledge mechanism for reliability.

## Features

- Periodic temperature and humidity readings from the AHT20 sensor
- UDP transmission with acknowledgement system

## Requirements

- ESP32-S3-DevKitC-1
- Adafruit AHT20 sensor (I2C)
- ESP-IDF (v5.0 or higher)
- Linux/macOS (tested on Ubuntu)
- Remote UDP server to receive data

## Quick Start

1. Clone and set up [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html)
2. Clone project
  ```bash
  git clone https://github.com/CategoryCory/TempHumiditySensorProject.git
  cd TempHumiditySensorProject
  . $IDF_PATH/export.sh
  idf.py set-target esp32s3
  ```
3. Create config file
  ```bash
  cp main/config.example.h config.h
  ```
4. Replace values in `config.h` with your own settings
5. Build and flash
  ```bash
  idf.py build
  idf.py -p /dev/<your-port> flash
  ```
  > Tip: If flashing fails, press and hold `BOOT`, tap `RESET`, and release `BOOT` to enter bootloader mode.

## Example Output

Example message sent via UDP:

```json
{
  "temperature_celsius": 28.0,
  "relative_humidity": 53.2,
}
```

UDP receiver must acknowledge receipt, or the device will retry up to 3 times.

## Basic Usage

1. Start your UDP server or test receiver script.
2. Flash the ESP32 firmware as described above.
3. On successful Wi-Fi connection, the device will begin sending sensor data at regular intervals, as determined by the `READ_SENSOR_SECONDS` value in `config.h`.
4. Each message must be acknowledged with a simple reply (e.g., "OK").

## License

MIT License
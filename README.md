# ESP32 BLE Air Monitor — Track A

## Project overview

This project uses an ESP32 and a BMP280 sensor to collect temperature readings and transmit them over Bluetooth Low Energy (BLE). A Python program on a computer connects to the ESP32, reads the BLE characteristics, displays the readings, and saves them to a CSV file.

**Device name:** `AQM-Kavin`

## Track selected

**Track A — ESP32 BLE air monitor**

## Hardware and wiring

### Components

- ESP32 development board
- BMP280 temperature and pressure sensor module
- USB cable for programming and powering the ESP32
- Computer running Python
- Phone with nRF Connect for BLE testing

### I²C connections

| BMP280 pin | ESP32 pin |
|---|---|
| SDA | GPIO 25 |
| SCL | GPIO 26 |
| VCC | Connect to the voltage supported by your BMP280 breakout board |
| GND | GND |

**Sensor limitation:** The BMP280 measures temperature and pressure, not humidity. In this firmware, humidity is a simulated value of `50.0%`; it is not a measured humidity reading.

## Software and files

The repository contains:

- `esp32_firmware.ino` — ESP32 firmware that reads the sensor and provides BLE characteristics.
- `aqm_logger.py` — Python BLE client that reads values and saves them to CSV.
- `aqm_readings.csv` — example output file, included.
- `AQM_Track_A_Demo.mp4` — demonstration video, included.
- `AQM_Track_A_CSV_Logging_Demo.mp4` — CSV logging demonstration, included separately.

## BLE service and characteristics

The ESP32 advertises the BLE device as `AQM-Kavin`.

| Service or characteristic | UUID | Properties |
|---|---|---|
| Air Monitor service | `7a1f0001-8e23-4b8c-a123-456789abcdef` | Service |
| Temperature | `7a1f0002-8e23-4b8c-a123-456789abcdef` | Read, Notify |
| Humidity | `7a1f0003-8e23-4b8c-a123-456789abcdef` | Read, Notify |
| Sampling interval | `7a1f0004-8e23-4b8c-a123-456789abcdef` | Read, Write |
| Temperature alert | `7a1f0005-8e23-4b8c-a123-456789abcdef` | Read, Notify |

The temperature alert is based on the 5-sample moving-average temperature. It is `1` when the average temperature is greater than 31.0°C and `0` otherwise.## BLE service and characteristics

## Temperature Alert

The ESP32 provides a temperature alert through a dedicated BLE characteristic.

- **Characteristic UUID:** `7a1f0005-8e23-4b8c-a123-456789abcdef`
- **Properties:** Read and Notify
- **Alert threshold:** 31.0°C
- **Alert value:** `1` = ON, `0` = OFF

The alert is based on the 5-sample moving-average temperature. When the average temperature is greater than 31.10°C, the characteristic value becomes `1`; otherwise, it remains `0`.

The alert can be viewed in nRF Connect by reading or subscribing to the Temperature Alert characteristic. The Python BLE logger also records the alert value in the CSV file when the characteristic is available.

## Build and run

### 1. Upload the ESP32 firmware

1. Install Arduino IDE and the ESP32 board support package.
2. Install the libraries used by `esp32_firmware.ino`. Check the `#include` lines in the source file for the exact library names.
3. Open `esp32_firmware.ino` in Arduino IDE.
4. Select the correct ESP32 board and its connected serial port.
5. Upload the firmware to the ESP32.
6. Open nRF Connect on a phone, scan for `AQM-Kavin`, and connect.

The firmware attempts to find the BMP280 at I²C address `0x76` or `0x77`.

### 2. Run the Python BLE logger

Install Python, then install Bleak:

```bash
python -m pip install bleak
```

Run the logger from the folder containing `aqm_logger.py`:

```bash
python aqm_logger.py
```

The logger scans for `AQM-Kavin`, connects to the ESP32, reads the temperature and humidity characteristics, displays timestamped readings, and writes them to `aqm_readings.csv`.

The Python logger’s polling delay is controlled by the sleep value in `aqm_logger.py`. This is separate from the ESP32’s sensor sampling interval.

## Testing and proof of operation

Record only tests that you have actually completed. The following tests can be used to demonstrate the project:

- Confirm that `AQM-Kavin` appears in nRF Connect.
- Connect to the ESP32 and read the temperature, humidity, and sampling-interval characteristics.
- Run `aqm_logger.py` and show readings appearing in the terminal.
- Open `aqm_readings.csv` and confirm that the logged readings are present.
- If testing interval changes, write a new value to the sampling-interval characteristic and verify the updated value and sampling behavior.

Proof files, if included:

- `AQM_Track_A_Demo.mp4` — BLE connection and nRF Connect demonstration.
- `AQM_Track_A_CSV_Logging_Demo.mp4` — logger and CSV output demonstration.
- `AQM_Track_A_Readings.csv` — sample CSV output.

## Issues and limitations

- Humidity is simulated at `50.0%`; it is not measured by the BMP280.
- The Python logger’s polling interval and the ESP32’s sampling interval are separate settings.
- If the BMP280 is not detected or is wired incorrectly, valid sensor readings may not be available.
- BLE connection and operation depend on the computer or phone’s Bluetooth support.

## AI usage 

I used ChatGPT to help interpret the task requirements, organize the README, and troubleshoot the BLE logger and CSV workflow. I tested the firmware and logger on my hardware and checked the documentation against my project setup. I retained only details that matched my implementation and identified simulated humidity as a limitation.



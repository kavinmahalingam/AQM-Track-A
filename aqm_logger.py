import asyncio
import csv
from datetime import datetime

from bleak import BleakClient, BleakScanner

DEVICE_NAME = "AQM-Kavin"

TEMP_UUID = "7a1f0002-8e23-4b8c-a123-456789abcdef"
HUMIDITY_UUID = "7a1f0003-8e23-4b8c-a123-456789abcdef"

CSV_FILE = "aqm_readings.csv"
LOG_INTERVAL_SECONDS = 2

# Match the ESP32 firmware threshold.
ALERT_THRESHOLD_C = 31.0


def decode_value(data):
    return data.decode("utf-8").strip()


async def main():
    print(f"Searching for {DEVICE_NAME}...")
    device = await BleakScanner.find_device_by_name(
        DEVICE_NAME, timeout=15
    )

    if device is None:
        print(
            "Device not found. Check that the ESP32 is powered on "
            "and advertising."
        )
        return

    print(f"Found {DEVICE_NAME}. Connecting...")

    async with BleakClient(device) as client:
        print("Connected. Discovered GATT services:")

        for service in client.services:
            print(f"Service: {service.uuid}")
            for characteristic in service.characteristics:
                print(f"  Characteristic: {characteristic.uuid}")

        temp_char = client.services.get_characteristic(TEMP_UUID)
        humidity_char = client.services.get_characteristic(HUMIDITY_UUID)

        if temp_char is None or humidity_char is None:
            print("ERROR: Temperature or humidity characteristic is missing.")
            return

        print(
            f"Logging every {LOG_INTERVAL_SECONDS} seconds. "
            f"Alert threshold: {ALERT_THRESHOLD_C:.1f} C. "
            "Press Ctrl+C to stop."
        )

        with open(CSV_FILE, "w", newline="", encoding="utf-8") as file:
            writer = csv.writer(file)
            writer.writerow(
                [
                    "Time",
                    "Temperature_C",
                    "Humidity_Percent",
                    "Temperature_Alert",
                ]
            )

            while True:
                try:
                    temp_data = await client.read_gatt_char(temp_char)
                    humidity_data = await client.read_gatt_char(humidity_char)

                    temperature = float(decode_value(temp_data))
                    humidity = float(decode_value(humidity_data))

                    # Temperature received from the ESP32 is already
                    # filtered by its 5-sample moving average.
                    alert = "1" if temperature > ALERT_THRESHOLD_C else "0"
                    alert_text = "ON" if alert == "1" else "OFF"

                    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

                    print(
                        f"{timestamp} | Temperature: {temperature:.2f} C"
                        f" | Humidity: {humidity:.2f} %"
                        f" | Alert: {alert_text}"
                    )

                    writer.writerow(
                        [timestamp, temperature, humidity, alert]
                    )
                    file.flush()

                    await asyncio.sleep(LOG_INTERVAL_SECONDS)

                except asyncio.CancelledError:
                    raise
                except Exception as error:
                    print(f"Read failed: {error}")
                    await asyncio.sleep(LOG_INTERVAL_SECONDS)


try:
    asyncio.run(main())
except KeyboardInterrupt:
    print(f"\nStopped. Readings saved in {CSV_FILE}.")

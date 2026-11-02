import asyncio
import csv
from datetime import datetime
from bleak import BleakClient, BleakScanner

DEVICE_NAME = "AQM-Kavin"

TEMP_UUID = "7a1f0002-8e23-4b8c-a123-456789abcdef"
HUMIDITY_UUID = "7a1f0003-8e23-4b8c-a123-456789abcdef"

CSV_FILE = "aqm_readings.csv"


def decode_reading(data):
    return float(data.decode("utf-8").strip())


async def main():
    print(f"Searching for {DEVICE_NAME}...")
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=15)

    if device is None:
        print("Device not found. Check that the ESP32 is powered on and advertising.")
        return

    print(f"Found {DEVICE_NAME}. Connecting...")

    async with BleakClient(device) as client:
        print("Connected! Reading sensor values...")

        with open(CSV_FILE, "w", newline="") as file:
            writer = csv.writer(file)
            writer.writerow(["Time", "Temperature_C", "Humidity_Percent"])

            while True:
                try:
                    temp_data = await client.read_gatt_char(TEMP_UUID)
                    humidity_data = await client.read_gatt_char(HUMIDITY_UUID)

                    temperature = decode_reading(temp_data)
                    humidity = decode_reading(humidity_data)
                    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

                    print(
                        f"{timestamp} | Temperature: {temperature:.2f} °C"
                        f" | Humidity: {humidity:.2f} %"
                    )

                    writer.writerow([timestamp, temperature, humidity])
                    file.flush()

                    await asyncio.sleep(2)

                except KeyboardInterrupt:
                    break


try:
    asyncio.run(main())
except KeyboardInterrupt:
    print("\nStopped. Your readings are saved in aqm_readings.csv.")

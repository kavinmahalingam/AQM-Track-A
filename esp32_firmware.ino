#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ---------- Hardware ----------
#define I2C_SDA 25
#define I2C_SCL 26

Adafruit_BMP280 bmp;

// ---------- BLE identifiers ----------
#define DEVICE_NAME       "AQM-Kavin"

#define SERVICE_UUID      "7a1f0001-8e23-4b8c-a123-456789abcdef"
#define TEMP_UUID         "7a1f0002-8e23-4b8c-a123-456789abcdef"
#define HUMIDITY_UUID     "7a1f0003-8e23-4b8c-a123-456789abcdef"
#define INTERVAL_UUID     "7a1f0004-8e23-4b8c-a123-456789abcdef"
#define ALERT_UUID        "7a1f0005-8e23-4b8c-a123-456789abcdef"

// ---------- Sampling and alert settings ----------
const uint8_t WINDOW_SIZE = 5;
const uint32_t DEFAULT_INTERVAL_MS = 2000;
const uint32_t MIN_INTERVAL_SECONDS = 1;
const uint32_t MAX_INTERVAL_SECONDS = 3600;

// Alert turns ON when the 5-sample average temperature is above this value.
const float TEMPERATURE_ALERT_THRESHOLD_C = 31.0;

float temperatureSamples[WINDOW_SIZE];
float humiditySamples[WINDOW_SIZE];

uint8_t sampleCount = 0;
uint8_t nextSampleIndex = 0;

float averageTemperature = NAN;
float averageHumidity = NAN;

uint32_t samplingIntervalMs = DEFAULT_INTERVAL_MS;
uint32_t lastSampleTime = 0;

BLECharacteristic *temperatureCharacteristic = nullptr;
BLECharacteristic *humidityCharacteristic = nullptr;
BLECharacteristic *intervalCharacteristic = nullptr;
BLECharacteristic *alertCharacteristic = nullptr;

bool deviceConnected = false;

// ---------- BLE connection callbacks ----------
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *server) override {
    deviceConnected = true;
    Serial.println("BLE client connected.");
  }

  void onDisconnect(BLEServer *server) override {
    deviceConnected = false;
    Serial.println("BLE client disconnected.");
    BLEDevice::startAdvertising();
  }
};

// ---------- Sampling interval callback ----------
class IntervalCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    String input = characteristic->getValue();
    input.trim();

    if (input.length() == 0) {
      Serial.println("Interval write ignored: empty value.");
      return;
    }

    char *endPointer = nullptr;
    long seconds = strtol(input.c_str(), &endPointer, 10);

    if (endPointer == input.c_str() ||
        *endPointer != '\0' ||
        seconds < MIN_INTERVAL_SECONDS ||
        seconds > MAX_INTERVAL_SECONDS) {
      Serial.println("Interval write ignored: use 1 to 3600 seconds.");
      return;
    }

    samplingIntervalMs = (uint32_t)seconds * 1000UL;
    characteristic->setValue(String(seconds).c_str());

    Serial.print("Sampling interval set to ");
    Serial.print(seconds);
    Serial.println(" seconds.");
  }
};

// ---------- Sensor functions ----------
float readSimulatedHumidity() {
  // BMP280 does not measure humidity. This is simulated data.
  return 50.0;
}

bool readSensor(float &temperatureC, float &humidityPercent) {
  temperatureC = bmp.readTemperature();
  humidityPercent = readSimulatedHumidity();

  return !isnan(temperatureC);
}

// ---------- Moving average ----------
void addSample(float temperatureC, float humidityPercent) {
  temperatureSamples[nextSampleIndex] = temperatureC;
  humiditySamples[nextSampleIndex] = humidityPercent;

  nextSampleIndex = (nextSampleIndex + 1) % WINDOW_SIZE;

  if (sampleCount < WINDOW_SIZE) {
    sampleCount++;
  }

  float temperatureTotal = 0.0;
  float humidityTotal = 0.0;

  for (uint8_t i = 0; i < sampleCount; i++) {
    temperatureTotal += temperatureSamples[i];
    humidityTotal += humiditySamples[i];
  }

  averageTemperature = temperatureTotal / sampleCount;
  averageHumidity = humidityTotal / sampleCount;
}

// ---------- Update BLE characteristics ----------
void updateCharacteristics() {
  if (isnan(averageTemperature) || isnan(averageHumidity)) {
    return;
  }

  char temperatureText[16];
  char humidityText[16];

  snprintf(
    temperatureText,
    sizeof(temperatureText),
    "%.2f",
    averageTemperature
  );

  snprintf(
    humidityText,
    sizeof(humidityText),
    "%.2f",
    averageHumidity
  );

  // Alert is based on the moving-average temperature.
  const char *alertValue =
    (averageTemperature > TEMPERATURE_ALERT_THRESHOLD_C) ? "1" : "0";

  temperatureCharacteristic->setValue(temperatureText);
  humidityCharacteristic->setValue(humidityText);
  alertCharacteristic->setValue(alertValue);

  if (deviceConnected) {
    temperatureCharacteristic->notify();
    humidityCharacteristic->notify();
    alertCharacteristic->notify();
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Starting AQM firmware...");

  // Start BLE before sensor initialization.
  Serial.println("Starting BLE...");
  BLEDevice::init(DEVICE_NAME);

  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService *service = server->createService(SERVICE_UUID);

  // Temperature: Read + Notify
  temperatureCharacteristic = service->createCharacteristic(
    TEMP_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  temperatureCharacteristic->addDescriptor(new BLE2902());
  temperatureCharacteristic->setValue("0.00");

  // Humidity: Read + Notify
  humidityCharacteristic = service->createCharacteristic(
    HUMIDITY_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  humidityCharacteristic->addDescriptor(new BLE2902());
  humidityCharacteristic->setValue("50.00");

  // Sampling interval: Read + Write
  intervalCharacteristic = service->createCharacteristic(
    INTERVAL_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE
  );
  intervalCharacteristic->setCallbacks(new IntervalCallbacks());
  intervalCharacteristic->setValue("2");

  // Temperature alert: Read + Notify
  // "1" = alert ON; "0" = alert OFF.
  alertCharacteristic = service->createCharacteristic(
    ALERT_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  alertCharacteristic->addDescriptor(new BLE2902());
  alertCharacteristic->setValue("0");

  service->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.println("BLE advertising started.");
  Serial.println("Device name: AQM-Kavin");

  // Initialize BMP280 after BLE has started.
  Serial.println("Initializing BMP280...");
  Wire.begin(I2C_SDA, I2C_SCL);

  bool sensorReady = bmp.begin(0x76);
  if (!sensorReady) {
    sensorReady = bmp.begin(0x77);
  }

  if (!sensorReady) {
    Serial.println("ERROR: BMP280 not detected.");
    return;
  }

  Serial.println("BMP280 detected.");

  bmp.setSampling(
    Adafruit_BMP280::MODE_NORMAL,
    Adafruit_BMP280::SAMPLING_X2,
    Adafruit_BMP280::SAMPLING_X16,
    Adafruit_BMP280::FILTER_X16,
    Adafruit_BMP280::STANDBY_MS_500
  );

  // Take first sample.
  float temperatureC;
  float humidityPercent;

  if (readSensor(temperatureC, humidityPercent)) {
    addSample(temperatureC, humidityPercent);
    updateCharacteristics();
    Serial.println("Initial sensor reading complete.");
  } else {
    Serial.println("Initial sensor reading failed.");
  }

  lastSampleTime = millis();
  Serial.println("Setup complete.");
}

// ---------- Main loop ----------
void loop() {
  uint32_t now = millis();

  if (now - lastSampleTime >= samplingIntervalMs) {
    lastSampleTime = now;

    float temperatureC;
    float humidityPercent;

    if (readSensor(temperatureC, humidityPercent)) {
      addSample(temperatureC, humidityPercent);
      updateCharacteristics();

      bool alertOn =
        averageTemperature > TEMPERATURE_ALERT_THRESHOLD_C;

      Serial.print("Temperature average: ");
      Serial.print(averageTemperature, 2);
      Serial.print(" C | Humidity average (simulated): ");
      Serial.print(averageHumidity, 2);
      Serial.print(" % | Alert: ");
      Serial.print(alertOn ? "ON" : "OFF");
      Serial.print(" | Threshold: ");
      Serial.print(TEMPERATURE_ALERT_THRESHOLD_C, 1);
      Serial.print(" C | Interval: ");
      Serial.print(samplingIntervalMs / 1000);
      Serial.println(" s");
    } else {
      Serial.println("Sensor read failed.");
    }
  }

  delay(10);
}

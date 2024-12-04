#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

const int relayPin = 2; // GPIO pin connected to the relay's IN pin
const int sensorPin = 34; // Soil moisture sensor O/P pin
int _moisture, sensor_analog;
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = pCharacteristic->getValue();
    if (value == "1") {
      digitalWrite(relayPin, HIGH); // Turn on the relay
    } else if (value == "0") {
      digitalWrite(relayPin, LOW); // Turn off the relay
    }
  }
};

void setup() {
  Serial.begin(115200);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW); // Ensure relay is off initially

  BLEDevice::init("ESP32_Relay");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->setValue("0");
  pService->start();

  pServer->getAdvertising()->start();
  Serial.println("Waiting for a client connection to notify...");
}

void loop() {
  if (deviceConnected) {
    sensor_analog = analogRead(sensorPin);
    _moisture = (100 - ((sensor_analog / 4095.00) * 100));
    Serial.print("Moisture = ");
    Serial.print(_moisture);  /* Print moisture on the serial window */
    Serial.println("%");

    // Send moisture data to the client
    pCharacteristic->setValue(String(_moisture));
    pCharacteristic->notify();

    delay(1000);  // Wait for 1 second before next reading
  }
}

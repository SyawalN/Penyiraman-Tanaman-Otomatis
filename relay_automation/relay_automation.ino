#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

const int relayPin = 2; // GPIO pin connected to the relay's IN pin
const int sensorPin = 34; // Soil moisture sensor O/P pin
int _moisture, sensor_analog;
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
BLECharacteristic* pStateCharacteristic = NULL;
bool deviceConnected = false;
String deviceState = "Device is [OFF]";

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define STATE_CHARACTERISTIC_UUID "b9a400e6-30dc-4bc5-913c-c208d518515b"

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    pServer->getAdvertising()->start();
  }
};

class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = pCharacteristic->getValue();
    if (value == "0") {
      // Turn of relay
      deviceState = "Device is [OFF]";
      digitalWrite(relayPin, HIGH);
    } else if (value == "1") {
      // Turn on relay
      deviceState = "Device is [ON]";
      digitalWrite(relayPin, LOW);
    }
  }
};

void setup() {
  Serial.begin(115200);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH); // Ensure relay is off initially

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

  // Kondisi Alat
  pStateCharacteristic = pService->createCharacteristic(
                      STATE_CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  // pStateCharacteristic->addDescriptor(new BLE2902());
  // pStateCharacteristic->getDescriptor(0)
  pStateCharacteristic->setValue(deviceState);
  
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
    pCharacteristic->setValue(deviceState);
    pCharacteristic->notify();

    // Kirim data kondisi alat
    Serial.println("State: " + pStateCharacteristic->getValue());
    pStateCharacteristic->setValue(deviceState);
    pStateCharacteristic->notify();

    delay(1000);  // Wait for 1 second before next reading
  }
}

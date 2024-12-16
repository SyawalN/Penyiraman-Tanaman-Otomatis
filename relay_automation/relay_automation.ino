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
bool relayEnabled = false; // Tracks whether relay control is active
bool userCommand = false;  // Tracks if the user wrote "1"
unsigned long relayStartTime = 0; // Time tracking for relay state changes
const unsigned long ON_DURATION = 10000;  // Relay ON time in ms
const unsigned long OFF_DURATION = 10000; // Relay OFF time in ms
const int MOISTURE_THRESHOLD = 30;        // Moisture threshold (%)
int relayState = 0;                       // 0 = OFF, 1 = ON, 2 = DELAY

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
      relayEnabled = false;
      userCommand = false;
      relayState = 0; // Reset relay state
    } else if (value == "1") {
      // Turn on relay
      deviceState = "Device is [ON]";
      digitalWrite(relayPin, LOW);
      relayEnabled = true;
      userCommand = true;
      relayStartTime = millis();
      relayState = 1; // Relay is ON
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
    pCharacteristic->setValue(String(_moisture));
    pCharacteristic->notify();

    // Kirim data kondisi alat
    Serial.println("State: " + pStateCharacteristic->getValue());
    pStateCharacteristic->setValue(deviceState);
    pStateCharacteristic->notify();

    // Relay state machine
    switch (relayState) {
      case 1: // Relay is ON
        if (millis() - relayStartTime >= ON_DURATION) {
          digitalWrite(relayPin, HIGH); // Turn OFF the relay
          relayState = 2; // Move to OFF_DELAY state
          deviceState = "Device is [OFF]";
          relayStartTime = millis();
          Serial.println("Relay turned OFF for delay.");
        }
        break;

      case 2: // Relay is OFF (delay state)
        if (millis() - relayStartTime >= OFF_DURATION) {
          if (_moisture > MOISTURE_THRESHOLD) {
            // Moisture too high, keep relay OFF
            relayState = 0; // Stop further actions
            relayEnabled = false;
            deviceState = "Device is [OFF]";
            Serial.println("Moisture > threshold, relay remains OFF.");
          } else {
            // Moisture is low, turn relay ON again
            digitalWrite(relayPin, LOW);
            relayState = 1; // Move to ON state
            deviceState = "Device is [ON]";
            relayStartTime = millis();
            Serial.println("Moisture <= threshold, relay turned ON again.");
          }
        }
        break;

      default: // Relay is OFF
        break;
    }

    delay(1000);  // Wait for 1 second before next reading
  }
}

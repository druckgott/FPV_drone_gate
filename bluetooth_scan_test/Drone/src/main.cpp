#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>

// Function to create advertisement data
void createAdvertisementData(BLEAdvertising *pAdvertising) {
  BLEAdvertisementData oAdvertisementData;

  // Set flags: BLE capable, no Bluetooth Classic
  oAdvertisementData.setFlags(ESP_BLE_ADV_FLAG_LIMIT_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

  // Custom data (UUID, Major, Minor)
  std::string strServiceData = "";
  strServiceData += (char)0x02;    // Length of the following data
  strServiceData += (char)0x15;    // iBeacon data type
  strServiceData += "12345678-1234-5678-1234-56789abcdef0"; // UUID
  strServiceData += (char)0x00;    // Major (high byte)
  strServiceData += (char)0x01;    // Major (low byte)
  strServiceData += (char)0x00;    // Minor (high byte)
  strServiceData += (char)0x01;    // Minor (low byte)
  strServiceData += (char)0xc5;    // RSSI (Signal strength)

  // Add custom data directly as std::string
  oAdvertisementData.addData(strServiceData);
  pAdvertising->setAdvertisementData(oAdvertisementData);
}

void setup() {
  Serial.begin(115200);
  Serial.print("Device name: ");
  Serial.println("Drone_03");

  // Initialize BLE
  BLEDevice::init("Drone_03");

  // Configure BLE advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();

  // Create the beacon data
  createAdvertisementData(pAdvertising);

  // Set the advertising interval to 20 ms (32 in 0.625 ms units)
  pAdvertising->setMinInterval(32); // 20 ms = 0x14 in 625 μs units
  pAdvertising->setMaxInterval(32); // 20 ms = 0x14 in 625 μs units

  // Start advertising
  pAdvertising->start();
  Serial.println("Beacon advertising every 20ms");
}

void loop() {
  // Nothing to do in loop, only advertising
}

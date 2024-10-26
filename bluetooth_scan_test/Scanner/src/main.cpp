#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <Arduino.h>


class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // Hole die Service-Daten vom beworbenen Gerät
    //std::string serviceData = advertisedDevice.getServiceData();
    std::string deviceName = advertisedDevice.getName();
    if (!deviceName.empty() && deviceName.find("Drone_") != std::string::npos) { // Verwende .empty() anstelle von !
    Serial.println("Scan-Ergebnis gefunden:");

    // Ausgabe des Gerätetyps und der RSSI-Signalstärke
    Serial.print("Gerätename: ");
    Serial.println(advertisedDevice.getName().c_str());
    Serial.print("Signalstärke (RSSI): ");
    Serial.println(advertisedDevice.getRSSI());
    }

    BLEDevice::getScan()->stop();
  }
};

void setup() {
  Serial.begin(115200);
  delay(3000); // 1 Sekunde warten
  Serial.println("Startet kontinuierlichen BLE-Scan...");

  // BLE initialisieren
  BLEDevice::init("");
  

  // BLE-Scan konfigurieren
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); // Aktives Scannen für genauere RSSI-Werte
  pBLEScan->setInterval(32);    // Scan-Intervall in 0.625ms-Einheiten
  pBLEScan->setWindow(32);       // Scan-Fenster in 0.625ms-Einheiten
  
}

void loop() {
  // Starte den Scan ohne zeitliche Begrenzung und bereinige die Ergebnisse direkt danach
  BLEDevice::getScan()->start(0.1, true); // kurzer Scan, um wiederholte Ergebnisse zu ermöglichen

  //int deviceCount = BLEDevice::getScan()->getResults().getCount();
  //Serial.print("Anzahl gefundener Geräte: ");
  //Serial.println(deviceCount);


  BLEDevice::getScan()->clearResults();    // Bereinige die Ergebnisse sofort für den nächsten Scan
}

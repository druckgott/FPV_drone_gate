#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

//#define DEBUG // Kommentiere diese Zeile aus, um Debugging-Ausgaben zu deaktivieren

const char* ssid = "LED_GATE_SERVER_01";           // SSID des Access Points (ESP8266)
const char* password = "12345678";                  // Passwort des Access Points (ESP8266)

// Empfänger MAC-Adresse eintragen (Beispiel MAC-Adresse, anpassen!)
uint8_t broadcastAddress[] = {0x4A, 0x3F, 0xDA, 0x7E, 0x58, 0x9F};

typedef struct {
  char deviceName[32];
  int rssi;
} DroneData;

DroneData droneData;



// Callback-Funktion für ESP-NOW-Sendebestätigung
void onSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  #ifdef DEBUG
  Serial.print("Sende-Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Erfolgreich" : "Fehlgeschlagen");
  #endif
}


// ESP-NOW initialisieren
void initESPNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("Fehler bei der Initialisierung von ESP-NOW");
    return;
  }
  esp_now_register_send_cb(onSent);
}



class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    std::string deviceName = advertisedDevice.getName();
    if (!deviceName.empty() && deviceName.find("Drone_") != std::string::npos) {
      int rssi = advertisedDevice.getRSSI();

      // DeviceName und RSSI in struct kopieren
      strncpy(droneData.deviceName, deviceName.c_str(), sizeof(droneData.deviceName));
      droneData.rssi = rssi;

      // Daten per ESP-NOW senden
      esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &droneData, sizeof(droneData));

      #ifdef DEBUG
        Serial.println("Scan-Ergebnis gefunden:");
        Serial.print("Gerätename: ");
        Serial.println(deviceName.c_str());
        Serial.print("Signalstärke (RSSI): ");
        Serial.println(rssi);
        Serial.print("Nachricht gesendet: ");
        Serial.print("ESP_satellit_01: ");
        Serial.print(deviceName.c_str());
        Serial.print(" RSSI: ");
        Serial.println(rssi);
        Serial.println(result == ESP_OK ? "Erfolgreich gesendet" : "Senden fehlgeschlagen");
      #endif
    }

    BLEDevice::getScan()->stop();
  }
};

void setup() {
  Serial.begin(115200);
  delay(3000);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Verbinde mit dem Access Point...");
  }
  Serial.println("Verbunden mit dem Access Point");

  // ESP-NOW initialisieren
  WiFi.mode(WIFI_STA);
  initESPNow();

  // Empfänger hinzufügen
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Fehler beim Hinzufügen des Peers");
    return;
  }

  Serial.println("Startet kontinuierlichen BLE-Scan...");

  // BLE initialisieren
  BLEDevice::init("");

  // BLE-Scan konfigurieren
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); // Aktives Scannen für genauere RSSI-Werte
  pBLEScan->setInterval(32);      // Scan-Intervall in 0.625ms-Einheiten
  pBLEScan->setWindow(32);        // Scan-Fenster in 0.625ms-Einheiten
}

void loop() {
  // Starte den Scan ohne zeitliche Begrenzung und bereinige die Ergebnisse direkt danach
  BLEDevice::getScan()->start(0.1, true); // kurzer Scan, um wiederholte Ergebnisse zu ermöglichen

  BLEDevice::getScan()->clearResults();    // Bereinige die Ergebnisse sofort für den nächsten Scan
}

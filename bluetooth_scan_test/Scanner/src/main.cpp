#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <string>  // In Arduino wird dies meist automatisch eingebunden

//#define DEBUG // Kommentiere diese Zeile aus, um Debugging-Ausgaben zu deaktivieren

std::string deviceSuffix = "_02";  // Definiere das Suffix, z.B. "_01", "_02", etc.
const char* ssid = "LED_GATE_SERVER_01";           // SSID des Access Points (ESP8266)
const char* password = "12345678";                  // Passwort des Access Points (ESP8266)

// Empfänger MAC-Adresse eintragen (Beispiel MAC-Adresse, anpassen!)
uint8_t broadcastAddress[] = {0x4A, 0x3F, 0xDA, 0x7E, 0x58, 0x9F};

// Speichert die MAC-Adresse des ESP32
//uint8_t esp32MacAddress[6];

typedef struct {
  char deviceName[32];
  int rssi;
  int count; // Zähler hinzufügen
  //uint8_t macAddress[6]; // Array zur Speicherung der MAC-Adresse
} DroneData;

DroneData droneData;
int count = 0; // Zählervariable

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

      // Kombiniere den Gerätenamen mit dem Suffix
      std::string fullDeviceName = deviceName + deviceSuffix;

      // DeviceName und RSSI in struct kopieren
      strncpy(droneData.deviceName, fullDeviceName.c_str(), sizeof(droneData.deviceName));
      droneData.rssi = rssi;
      droneData.count = count; // Zählerstand in die Struktur kopieren
      //memcpy(droneData.macAddress, esp32MacAddress, 6); // MAC-Adresse kopieren

      // Daten per ESP-NOW senden
      esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &droneData, sizeof(droneData));

      #ifdef DEBUG
        Serial.println("Scan-Ergebnis gefunden:");
        Serial.print("Gerätename: ");
        Serial.println(fullDeviceName.c_str());
        Serial.print("Signalstärke (RSSI): ");
        Serial.println(rssi);
        Serial.print("Zählerstand: ");
        Serial.println(count);
        Serial.print("Nachricht gesendet: ");
        Serial.print("ESP_satellit_01: ");
        Serial.print(fullDeviceName.c_str());
        Serial.print(" RSSI: ");
        Serial.println(rssi);
        Serial.println(result == ESP_OK ? "Erfolgreich gesendet" : "Senden fehlgeschlagen");
      #endif
    }

    BLEDevice::getScan()->stop();
  }
};

void connectToWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Verbinde mit dem Access Point...");
  }
  Serial.println("Verbunden mit dem Access Point");
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  connectToWiFi(); // WiFi-Verbindung aufbauen

  // Gebe die MAC-Adresse des ESP32 aus
  Serial.print("MAC-Adresse des ESP32: ");
  Serial.println(WiFi.macAddress());

    // Speichere die MAC-Adresse des ESP32
  //WiFi.macAddress(esp32MacAddress);

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

  // Überprüfe die WiFi-Verbindung
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Verbindung verloren. Versuche, erneut zu verbinden...");
    connectToWiFi(); // WiFi erneut verbinden
  }
  // Zähler hochzählen
  count++;
  if (count > 10000) {
    count = 0; // Zähler zurücksetzen
  }

  // Starte den Scan ohne zeitliche Begrenzung und bereinige die Ergebnisse direkt danach
  BLEDevice::getScan()->start(0.1, true); // kurzer Scan, um wiederholte Ergebnisse zu ermöglichen

  BLEDevice::getScan()->clearResults();    // Bereinige die Ergebnisse sofort für den nächsten Scan
}

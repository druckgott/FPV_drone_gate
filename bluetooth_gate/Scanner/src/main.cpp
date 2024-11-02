#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <string>  // In Arduino wird dies meist automatisch eingebunden

//#define DEBUG // Kommentiere diese Zeile aus, um Debugging-Ausgaben zu deaktivieren

std::string deviceSuffix = "_03";  // Definiere das Suffix, z.B. "_01", "_02", etc.
int CHANNEL = 1;
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Broadcast-Adresse

typedef struct {
  char deviceName[32];
  int rssi;
  int count; // Zähler hinzufügen
} DroneData;

typedef struct struct_message {
    char msg[50];
} struct_message;

DroneData droneData;
// Create a struct_message called outgoingReadings to hold outgoing data        
struct_message incomingReadings;
int count = 0; // Zählervariable
// Variable to store if sending data was successful
String success;

bool canSend = false;

// Callback when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len)
{
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
  #ifdef DEBUG
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("Data: "); Serial.println(incomingReadings.msg);
  #endif
  // Erstellen Sie die vollständige Nachricht
  char expectedMsg[20]; // Puffer für die erwartete Nachricht, anpassen wenn nötig
  snprintf(expectedMsg, sizeof(expectedMsg), "REQUEST%s", deviceSuffix.c_str()); // "REQUEST" + deviceSuffix
  #ifdef DEBUG
  Serial.print("expectedMsg: "); Serial.println(expectedMsg);
  #endif
  if (strcmp(incomingReadings.msg, expectedMsg) == 0) {
    #ifdef DEBUG
    Serial.println("Senden aktiv ");
    #endif
    canSend = true;
  }
}

// Callback-Funktion für ESP-NOW-Sendebestätigung
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  #ifdef DEBUG
  Serial.print("Last Packet Send Status:");
  #endif
  //achtung eine Zeile muss drin bleiben sonst funktioniert das send nicht
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
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
      #endif
    }

    BLEDevice::getScan()->stop();
  }
};

void setup() {
  Serial.begin(115200);
  delay(6000);

  // ESP-NOW initialisieren
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);           
  // MAC-Adresse ausgeben
  Serial.print("MAC-Adresse: ");
  Serial.println(WiFi.macAddress());

  // IP-Adresse ausgeben
  Serial.print("IP-Adresse: ");
  Serial.println(WiFi.localIP());

  // WLAN-Kanal ausgeben
  Serial.print("WLAN-Kanal: ");
  Serial.println(WiFi.channel());
  
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = CHANNEL;  
  peerInfo.encrypt = false;

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);

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
  if (canSend) {
    // Zähler hochzählen
    count++;
    if (count > 10000) {
      count = 0; // Zähler zurücksetzen
    }
    #ifdef DEBUG
    Serial.println("Darf senden... " + String(count));
    #endif
    // Starte den Scan ohne zeitliche Begrenzung und bereinige die Ergebnisse direkt danach
    BLEDevice::getScan()->start(0.1, true); // kurzer Scan, um wiederholte Ergebnisse zu ermöglichen

    // Daten per ESP-NOW senden
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &droneData, sizeof(droneData));

    BLEDevice::getScan()->clearResults();    // Bereinige die Ergebnisse sofort für den nächsten Scan
    //Serial.println("Daten werden gesendet...");
    canSend = false;
  }
}

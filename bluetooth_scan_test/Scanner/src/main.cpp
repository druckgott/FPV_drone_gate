#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#define DEBUG // Kommentiere diese Zeile aus, um Debugging-Ausgaben zu deaktivieren

const char* ssid = "LED_GATE_SERVER_01";           // SSID des Access Points (ESP8266)
const char* password = "12345678";                  // Passwort des Access Points (ESP8266)
WiFiUDP udp;
const char* udpAddress = "192.168.4.1";             // IP-Adresse des ESP8266 (AP)
const int udpPort = 1234;

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    std::string deviceName = advertisedDevice.getName();
    if (!deviceName.empty() && deviceName.find("Drone_") != std::string::npos) {
      int rssi = advertisedDevice.getRSSI();

      // Erstelle die Nachricht im Format "ESP_satellit_01:Drone_XX:rssi"
      uint8_t message[6]; // Beispiel: 1 Byte für Prefix + 2 Bytes für ID + 2 Bytes für RSSI
      message[0] = 'E'; // 'E' für ESP_satellit
      message[1] = deviceName[6] - '0'; // Drohnen-ID (nur eine Ziffer für Demo)
      message[2] = deviceName[7] - '0'; // Drohnen-ID (nur eine Ziffer für Demo)
      message[3] = (uint8_t)(rssi + 200); // RSSI Offset um 200 (von -200 bis 31)
      message[4] = 0; // Reservierter Platz (z.B. für zukünftige Verwendung)
      message[5] = 0; // Reservierter Platz (z.B. für zukünftige Verwendung)

      // Sende die Nachricht per UDP
      udp.beginPacket(udpAddress, udpPort);
      udp.write(message, sizeof(message)); // Korrekte Verwendung der write-Funktion
      udp.endPacket();

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
      #endif
    }

    BLEDevice::getScan()->stop();
  }
};

void setup() {
  Serial.begin(115200);
  delay(3000); // 1 Sekunde warten

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Verbinde mit dem Access Point...");
  }
  Serial.println("Verbunden mit dem Access Point");

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

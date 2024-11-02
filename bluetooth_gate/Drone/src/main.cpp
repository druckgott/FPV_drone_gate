#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>

#define EEPROM_SIZE 10
#define PREFIX_ADDRESS 0 // EEPROM-Adresse für den Präfix

WebServer server(80);
String droneName = "Drone_03"; // Standard-Drohnennamen

// Funktion zur Erstellung von Werbedaten
void createAdvertisementData(BLEAdvertising *pAdvertising) {
  BLEAdvertisementData oAdvertisementData;

  // Setze Flags: BLE-fähig, kein Bluetooth Classic
  oAdvertisementData.setFlags(ESP_BLE_ADV_FLAG_LIMIT_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

  // Benutzerdefinierte Daten (UUID, Major, Minor)
  std::string strServiceData = "";
  strServiceData += (char)0x02;    // Länge der folgenden Daten
  strServiceData += (char)0x15;    // iBeacon-Datentyp
  strServiceData += "12345678-1234-5678-1234-56789abcdef0"; // UUID
  strServiceData += (char)0x00;    // Major (hoher Byte)
  strServiceData += (char)0x01;    // Major (niedriger Byte)
  strServiceData += (char)0x00;    // Minor (hoher Byte)
  strServiceData += (char)0x01;    // Minor (niedriger Byte)
  strServiceData += (char)0xc5;    // RSSI (Signalstärke)

  // Füge benutzerdefinierte Daten als std::string hinzu
  oAdvertisementData.addData(strServiceData);
  pAdvertising->setAdvertisementData(oAdvertisementData);
}

// HTML-Konfigurationsseite
const char* configPage = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta charset="UTF-8">
  <title>Drone Configuration</title>
</head>
<body>
  <h1>Configure Drone Name</h1>
  <form action="/setName" method="POST">
    <label for="prefix">Prefix (01-09): </label>
    <select id="prefix" name="prefix" required>
      <option value="01">01</option>
      <option value="02">02</option>
      <option value="03">03</option>
      <option value="04">04</option>
      <option value="05">05</option>
      <option value="06">06</option>
      <option value="07">07</option>
      <option value="08">08</option>
      <option value="09">09</option>
    </select>
    <input type="submit" value="Set">
  </form>
</body>
</html>
)rawliteral";

// Handler für die Konfigurationsseite
void handleRoot() {
  server.send(200, "text/html", configPage);
}

// Handler für das Setzen des Drohnennamens
void handleSetName() {
  if (server.arg("prefix").length() > 0) {
    String prefix = server.arg("prefix");
    droneName = "Drone_" + prefix; // Setze den neuen Namen
    EEPROM.write(PREFIX_ADDRESS, prefix.toInt()); // Speichere den Präfix im EEPROM
    EEPROM.commit(); // Schreibe Änderungen ins EEPROM
    Serial.print("New Drone name: ");
    Serial.println(droneName);
    server.send(200, "text/html", "<h1>Name gesetzt auf: " + droneName + "</h1><a href='/'>Zurück</a>");
    delay(1000);  // Small delay before restarting the ESP
    ESP.restart();  // Restart ESP
  } else {
    server.send(400, "text/plain", "Prefix fehlt");
  }
}

void setup() {
  Serial.begin(115200);

  // Initialisiere EEPROM
  EEPROM.begin(EEPROM_SIZE);
  int prefix = EEPROM.read(PREFIX_ADDRESS);
  if (prefix >= 0 && prefix <= 9) {
    droneName = "Drone_0" + String(prefix);
  } else {
    droneName = "Drone_03"; // Standardname
    EEPROM.write(PREFIX_ADDRESS, 3); // Setze Standardwert
    EEPROM.commit();
  }
  
  // Initialisiere BLE
  BLEDevice::init(droneName.c_str());
  WiFi.softAP(droneName.c_str()); // Setze die Wi-Fi SSID
  
  // Setze die Bluetooth TX-Leistung
  esp_err_t result = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9); // Setze auf +3 dBm
  if (result == ESP_OK) {
    Serial.println("Bluetooth TX power set successfully");
  } else {
    Serial.println("Failed to set Bluetooth TX power");
  }

  // Konfiguriere BLE-Werbung
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();

  // Erstelle die Beacon-Daten
  createAdvertisementData(pAdvertising);

  // Setze das Werbeintervall auf 20 ms (32 in 0.625 ms Einheiten)
  pAdvertising->setMinInterval(32); // 20 ms = 0x14 in 625 μs Einheiten
  pAdvertising->setMaxInterval(32); // 20 ms = 0x14 in 625 μs Einheiten

  // Starte die Werbung
  pAdvertising->start();
  Serial.println("Beacon advertising every 20ms");

  // Routen für den Webserver
  server.on("/", HTTP_GET, handleRoot);
  server.on("/setName", HTTP_POST, handleSetName);
  
  // Starte den Webserver
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient(); // Verarbeite eingehende Client-Anfragen
}

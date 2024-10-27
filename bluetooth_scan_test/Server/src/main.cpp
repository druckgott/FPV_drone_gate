#include <ESP8266WiFi.h>
#include <espnow.h>

const char* ssid = "LED_GATE_SERVER_01";           // SSID des Access Points
const char* password = "12345678";                  // Passwort des Access Points

typedef struct {
  char deviceName[32];
  int rssi;
} DroneData;

// Callback-Funktion für den Empfang von ESP-NOW-Daten
void onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  DroneData receivedData;
  memcpy(&receivedData, incomingData, sizeof(receivedData));

  Serial.print("Daten empfangen von: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", mac[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();

  Serial.print("Gerätename: ");
  Serial.println(receivedData.deviceName);
  Serial.print("RSSI: ");
  Serial.println(receivedData.rssi);
}

void setup() {
  Serial.begin(115200);
  delay(6000);

  // AP-Modus starten
  WiFi.softAP(ssid, password);
  Serial.println("Access Point gestartet");

  // Eigene MAC-Adresse anzeigen
  Serial.print("MAC-Adresse: ");
  Serial.println(WiFi.softAPmacAddress());

  // ESP-NOW initialisieren
  if (esp_now_init() != 0) {
    Serial.println("Fehler bei der Initialisierung von ESP-NOW");
    return;
  }

  // Callback für empfangene Daten registrieren
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(onDataRecv);

  Serial.println("Warte auf ESP-NOW-Nachrichten...");
}

void loop() {
  // ESP-NOW arbeitet mit Callback, daher keine Logik im Haupt-Loop nötig
  handleClient(); // Clientanfragen bearbeiten
}

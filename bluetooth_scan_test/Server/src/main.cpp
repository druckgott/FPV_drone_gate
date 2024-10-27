#include <ESP8266WiFi.h>
#include <espnow.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

const char* ssid = "LED_GATE_SERVER_01";           // SSID des Access Points
const char* password = "12345678";                  // Passwort des Access Points

#define MAX_DRONES 10  // Maximalanzahl an Drohnen, die gespeichert werden können

typedef struct {
  char deviceName[32];
  int rssi;
  int count; // Counter für Drohnen
} DroneData;

DroneData drones[MAX_DRONES];  // Array zum Speichern von Drohnendaten
int droneCount = 0;             // Zähler für die Anzahl der gespeicherten Drohnen

// Funktion zum Speichern oder Aktualisieren empfangener Daten
void storeDroneData(DroneData receivedData) {
  // Überprüfen, ob die Drohne bereits in der Liste ist
  for (int i = 0; i < droneCount; i++) {
    if (strcmp(drones[i].deviceName, receivedData.deviceName) == 0) {
      drones[i].rssi = receivedData.rssi; // RSSI-Wert aktualisieren
      drones[i].count = receivedData.count; // Counter erhöhen
      return; // Aktualisierung vorgenommen, keine weitere Speicherung nötig
    }
  }
  
  // Wenn die Drohne neu ist und der Maximalwert noch nicht erreicht ist
  if (droneCount < MAX_DRONES) {
    drones[droneCount++] = receivedData; // Drohne hinzufügen
    drones[droneCount - 1].count = 1; // Counter auf 1 setzen
  }
}

// Callback-Funktion für den Empfang von ESP-NOW-Daten
void onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  DroneData receivedData;
  memcpy(&receivedData, incomingData, sizeof(receivedData));

  Serial.print("Daten empfangen von: ");
  for (int i = 0; i < 6; i++) {
      Serial.printf("%02X", mac[i]);
      if (i < 5) Serial.print(":");
  }
  Serial.printf(" | Gerätename: %s | RSSI: %d | Counter: %d\n", receivedData.deviceName, receivedData.rssi, receivedData.count);

  // Drohne zu den Daten hinzufügen oder aktualisieren
  storeDroneData(receivedData);
}

// Funktion zum Erstellen des JSON-Dokuments für die Drohnendaten
String getDroneDataJson() {
  StaticJsonDocument<1024> jsonDoc;
  JsonArray jsonDrones = jsonDoc.createNestedArray("drones");

  for (int i = 0; i < droneCount; i++) {
    JsonObject drone = jsonDrones.createNestedObject();
    drone["deviceName"] = drones[i].deviceName;
    drone["rssi"] = drones[i].rssi;
    drone["count"] = drones[i].count; // Counter in das JSON einfügen
  }

  String json;
  serializeJson(jsonDoc, json);
  return json; // Rückgabe des JSON-Strings
}

ESP8266WebServer server(80);

// HTML-Seite für die Anzeige
const char* htmlPage = R"rawl(
<!DOCTYPE html>
<html>
<head>
  <title>Drone Data</title>
  <style>
    table {
      width: 100%;
      border-collapse: collapse;
    }
    th, td {
      border: 1px solid black;
      padding: 8px;
      text-align: left;
    }
    th {
      background-color: #f2f2f2;
    }
  </style>
  <script>
    setInterval(function() {
      fetch('/droneData')
      .then(response => response.json())
      .then(data => {
        const tableBody = document.getElementById('droneList');
        tableBody.innerHTML = ''; // Tabelle leeren
        data.drones.forEach(drone => {
          const row = document.createElement('tr');
          row.innerHTML = `<td>${drone.deviceName}</td><td>${drone.rssi}</td><td>${drone.count}</td>`;
          tableBody.appendChild(row);
        });
      });
    }, 10); // Aktualisiere alle 0.01 Sekunde
  </script>
</head>
<body>
  <h1>Live Drone Data</h1>
  <table>
    <thead>
      <tr>
        <th>Gerätename</th>
        <th>RSSI</th>
        <th>Counter</th>
      </tr>
    </thead>
    <tbody id="droneList"></tbody>
  </table>
</body>
</html>
)rawl";

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

  // Webserver-Routen
  server.on("/", []() {
    server.send(200, "text/html", htmlPage);
  });

  server.on("/droneData", []() {
    String json = getDroneDataJson(); // Aufruf der Funktion zur Erstellung des JSON
    server.send(200, "application/json", json);
  });

  server.begin();
  Serial.println("Webserver gestartet");
}

void loop() {
  // ESP-NOW arbeitet mit Callback, daher keine Logik im Haupt-Loop nötig
  server.handleClient(); // Clientanfragen bearbeiten
}

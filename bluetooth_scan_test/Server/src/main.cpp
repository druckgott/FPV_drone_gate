#include <ESP8266WiFi.h>
#include <espnow.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>

//#define DEBUG // Kommentiere diese Zeile aus, um Debugging-Ausgaben zu deaktivieren

const char* ssid = "LED_GATE_SERVER_01";           // SSID des Access Points
const char* password = "12345678";                  // Passwort des Access Points

#define MAX_DRONES 10  // Maximalanzahl an Drohnen, die gespeichert werden können
#define MAX_VALUES 3   // Maximalanzahl der RSSI und Counter-Werte pro Drohne

#define THRESHOLD_RSSI -85 // Definierbarer RSSI-Grenzwert für "innerhalb des Bereichs"

// NeoPixel Konfiguration
#define NUM_LEDS 150
#define LED_PIN 2          // Pin für die NeoPixel-LEDs (GPIO2, D4)
int BRIGHTNESS = 60; // Helligkeit als Konstante definieren

// Struktur zum Speichern von Drohnendaten
typedef struct {
  char deviceName[32];
  int rssi[MAX_VALUES];  // Array für bis zu 3 RSSI-Werte
  int count[MAX_VALUES];  // Array für bis zu 3 Counter-Werte
  int failed_value[MAX_VALUES];  // Array für bis zu 3 Counter-Werte
  int currentIndex;      // Aktueller Index für den nächsten Wert
  bool isInside;
} DroneData;

// Struktur zum Empfangen von Drohnendaten
typedef struct {
  char deviceName[32];
  int rssi;             // RSSI-Wert
  int count;            // Counter für Drohnen
} ReceivedDroneData;

int closest_drone_index = -1;
int closest_RSSI = 31;

// Farben für jede Drone
uint32_t droneColors[10] = {
    0xFF0000, // Drone_01: Rot
    0xFFFF00, // Drone_02: Gelb
    0x00FF00, // Drone_03: Grün
    0x0000FF, // Drone_04: Blau
    0xFF00FF, // Drone_05: Magenta
    0x00FFFF, // Drone_06: Cyan
    0xFFA500, // Drone_07: Orange
    0x800080, // Drone_08: Lila
    0x4bb7d6, // Drone_09: Türkis
    0x808080  // Drone_10: Grau
};

DroneData drones[MAX_DRONES];  // Array zum Speichern von Drohnendaten
int droneCount = 0;             // Zähler für die Anzahl der gespeicherten Drohnen
int rssiThreshold = THRESHOLD_RSSI; // Speichere den Wert in einer int-Variable

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
ESP8266WebServer server(80);

void setAllLEDs(uint32_t color, uint8_t brightness) {
    strip.setBrightness(brightness); // Setzt die Helligkeit für alle LEDs
    
    for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, color); // Setzt jede LED auf die gleiche Farbe
    }
    
    strip.show(); // Aktualisiert die LEDs, damit die Farbe und Helligkeit angezeigt werden
}

// Funktion zur Berechnung, ob eine Drohne innerhalb des Bereichs ist
bool calculateIsInside(int rssiValues[]) {
  for (int i = 0; i < MAX_VALUES; i++) {
    if (rssiValues[i] < THRESHOLD_RSSI || rssiValues[i] >= 0) {
      return false;  // Außerhalb des Bereichs
    }
  }
  return true; // Innerhalb des Bereichs
}

// Funktion zur Berechnung des Mittelwerts der RSSI-Werte einer Drohne
float calculateAverageRSSI(int rssi[MAX_VALUES]) {
  int validCount = 0;
  float sum = 0;

  for (int i = 0; i < MAX_VALUES; i++) {
    if (rssi[i] != 0) { // Nur gültige Werte einbeziehen
      sum += rssi[i];
      validCount++;
    }
  }

  return (validCount > 0) ? sum / validCount : 0;
}

// Funktion zur Identifizierung der Drohne mit dem niedrigsten RSSI-Mittelwert
void findClosestDrone(int &closestIndex, float &minAverage) {
  minAverage = -200; // Startwert für minAverage, da RSSI negativ ist
  closestIndex = -1;

  for (int i = 0; i < droneCount; i++) {
    float avgRSSI = calculateAverageRSSI(drones[i].rssi);
    
    if (avgRSSI > minAverage && avgRSSI < 0) {
      minAverage = avgRSSI;
      closestIndex = i;
    }
  }
}

void updateLEDBasedOnRSSI(int closestDroneIndex, int closestRSSI) {
  if (closestDroneIndex >= 0 && closestDroneIndex < 10) {
    // Anzahl der leuchtenden LEDs bestimmen
    int ledCount = 0;
    
    if (closestRSSI >= 0) {
      ledCount = NUM_LEDS; // Alle LEDs leuchten
    } else if (closestRSSI <= -100) {
      ledCount = NUM_LEDS / 2; // 50% der LEDs leuchten
    } else {
      // Interpolation zwischen 0 und -100 für die LED-Anzahl
      ledCount = map(closestRSSI, -100, 0, NUM_LEDS / 2, NUM_LEDS);
    }

    // Setze die Farbe für die LEDs
    for (int i = 0; i < NUM_LEDS; i++) {
      if (i < ledCount) {
        strip.setPixelColor(i, droneColors[closestDroneIndex]); // Farbe für die aktive Drohne
      } else {
        strip.setPixelColor(i, strip.Color(0, 0, 0)); // LEDs ausschalten
      }
    }
    
    strip.setBrightness(BRIGHTNESS);
    strip.show(); // Aktualisiere die LEDs
  }
}


// Funktion zum Speichern oder Aktualisieren empfangener Daten
void storeDroneData(ReceivedDroneData receivedData) {
  // Extrahiere das Suffix, um den RSSI/Count-Index zu bestimmen
  int index = -1;
  if (strcmp(&receivedData.deviceName[strlen(receivedData.deviceName) - 3], "_01") == 0) index = 0;
  else if (strcmp(&receivedData.deviceName[strlen(receivedData.deviceName) - 3], "_02") == 0) index = 1;
  else if (strcmp(&receivedData.deviceName[strlen(receivedData.deviceName) - 3], "_03") == 0) index = 2;

  // Wenn das Suffix nicht gefunden wurde, abbrechen
  if (index == -1) return;

  // Überprüfen, ob die Drohne bereits in der Liste ist
  for (int i = 0; i < droneCount; i++) {
    if (strncmp(drones[i].deviceName, receivedData.deviceName, 8) == 0) {
      // Neuen RSSI- und Counter-Wert an der bestimmten Position speichern
      drones[i].rssi[index] = receivedData.rssi;
      drones[i].count[index] = receivedData.count;

      // failed_value aktualisieren, falls der RSSI-Wert den Schwellenwert nicht erreicht
      drones[i].failed_value[index] = (receivedData.rssi < THRESHOLD_RSSI) ? receivedData.rssi : 0;
      // isInside anhand der aktuellen RSSI-Werte neu berechnen
      drones[i].isInside = calculateIsInside(drones[i].rssi);

      // Aktualisieren der nächstgelegenen Drohne
      int closestDroneIndex;
      float closestRSSI = 31;
      findClosestDrone(closestDroneIndex, closestRSSI);
      if (closestDroneIndex != -1 && drones[droneCount].isInside) {
        #ifdef DEBUG
        Serial.print("Die nächste Drohne ist: ");
        Serial.println(drones[closestDroneIndex].deviceName);
        #endif
        closest_drone_index = closestDroneIndex;
        closest_RSSI = closestRSSI;
        updateLEDBasedOnRSSI(closestDroneIndex, closestRSSI);
      }

      return; // Aktualisierung vorgenommen, keine weitere Speicherung nötig
    }
  }

  // Wenn die Drohne noch nicht gespeichert ist, füge sie hinzu
  if (droneCount < MAX_DRONES) {
    // Kopiere die ersten 8 Zeichen von receivedData.deviceName
    strncpy(drones[droneCount].deviceName, receivedData.deviceName, 8);
    drones[droneCount].deviceName[8] = '\0'; // Nullterminator hinzufügen
    
    // Alle RSSI werte initialisieren mit 0
    memset(drones[droneCount].rssi, 0, sizeof(drones[droneCount].rssi));
    memset(drones[droneCount].count, 0, sizeof(drones[droneCount].count));
    // Speichere den neuen RSSI- und Counter-Wert an der bestimmten Position
    drones[droneCount].rssi[index] = receivedData.rssi;
    drones[droneCount].count[index] = receivedData.count;

    // failed_value initialisieren
    drones[droneCount].failed_value[index] = (receivedData.rssi < THRESHOLD_RSSI) ? receivedData.rssi : 0;
    // Berechne den isInside-Status bei erstmaligem Hinzufügen
    drones[droneCount].isInside = calculateIsInside(drones[droneCount].rssi);
    
    drones[droneCount].currentIndex = 1; // Setze den aktuellen Index auf 1
    
    // Aktualisieren der nächstgelegenen Drohne
    int closestDroneIndex;
    float closestRSSI = 31;
    findClosestDrone(closestDroneIndex, closestRSSI);
    if (closestDroneIndex != -1 && drones[droneCount].isInside) {
      #ifdef DEBUG
      Serial.print("Die nächste Drohne ist: ");
      Serial.println(drones[closestDroneIndex].deviceName);
      #endif
      closest_drone_index = closestDroneIndex;
      closest_RSSI = closestRSSI;
      updateLEDBasedOnRSSI(closestDroneIndex, closestRSSI);
      
    }

    droneCount++; // Erhöhe die Drohnennummer
  }

}

// Callback-Funktion für den Empfang von ESP-NOW-Daten
void onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  ReceivedDroneData receivedData;
  memcpy(&receivedData, incomingData, sizeof(receivedData));

  #ifdef DEBUG
  Serial.print("Daten empfangen von: ");
  for (int i = 0; i < 6; i++) {
      Serial.printf("%02X", mac[i]);
      if (i < 5) Serial.print(":");
  }
  Serial.printf(" | Gerätename: %s | RSSI: %d | Counter: %d\n", receivedData.deviceName, receivedData.rssi, receivedData.count);
  #endif
  // Drohne zu den Daten hinzufügen oder aktualisieren
  storeDroneData(receivedData);
}

// Funktion zum Erstellen des JSON-Dokuments für die Drohnendaten
String getDroneDataJson() {
  StaticJsonDocument<1024> jsonDoc;
  
  // Verwende jsonDoc["drones"].to<JsonArray>() anstelle von createNestedArray
  JsonArray jsonDrones = jsonDoc["drones"].to<JsonArray>();

  for (int i = 0; i < droneCount; i++) {
    // Verwende jsonDrones.add<JsonObject>() anstelle von createNestedObject
    JsonObject drone = jsonDrones.add<JsonObject>();
    
    // Füge nur die ersten 8 Zeichen von deviceName hinzu
    char shortDeviceName[9]; // Array für die ersten 8 Zeichen plus Nullterminator
    strncpy(shortDeviceName, drones[i].deviceName, 8);
    shortDeviceName[8] = '\0'; // Nullterminator hinzufügen
    drone["deviceName"] = shortDeviceName;

    // Verwende obj[key].to<JsonArray>() anstelle von createNestedArray für Arrays
    JsonArray rssiArray = drone["rssi"].to<JsonArray>();
    JsonArray countArray = drone["count"].to<JsonArray>();
    JsonArray failedValueArray = drone["failed_value"].to<JsonArray>();

    for (int j = 0; j < MAX_VALUES; j++) {
      rssiArray.add(drones[i].rssi[j]);
      countArray.add(drones[i].count[j]);
      failedValueArray.add(drones[i].failed_value[j]);
    }

    // isInside-Status direkt aus der Struktur übernehmen
    drone["isInside"] = drones[i].isInside;
    drone["closest"]["drone_index"] = closest_drone_index;
    drone["closest"]["rssi"] = closest_RSSI;
  }

  String json;
  serializeJson(jsonDoc, json);
  
  return json; // Rückgabe des JSON-Strings
}

// HTML-Seite für die Anzeige
const char* htmlPage = R"rawl(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
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
    .green-bg {
      background-color: #a8e6a1; // Grün für niedrigen RSSI
    }
    .red-bg {
      background-color: #f4a8a8; // Rot für hohen RSSI
    }
    .closest {
      background-color: lime;
      color: black;
    }
    .farther {
      background-color: yellow;
      color: black;
    }
  </style>
  <script>
      let THRESHOLD_RSSI_JAVA;

      // Hole den THRESHOLD_RSSI-Wert vom Server
      fetch('/threshold')
        .then(response => response.json())
        .then(data => {
          THRESHOLD_RSSI_JAVA = data.threshold;
        });


  setInterval(function() {
    fetch('/droneData')
      .then(response => response.json())
      .then(data => {
        const tableBody = document.getElementById('droneList');
        tableBody.innerHTML = ''; // Tabelle leeren

        data.drones.forEach((drone, index) => {
          const row = document.createElement('tr');

          // Setze die Klasse basierend auf "closest"
          const rowClass = (index === data.drones[0].closest.drone_index) ? 'closest' : 'farther';
          row.className = rowClass;

          // Bestimme die Klasse für die RSSI-Werte basierend auf THRESHOLD_RSSI_JAVA
          const rssiCells = drone.rssi.map(rssi => {
          // Überprüfen, ob rssi einen Wert hat, sonst rot setzen
            const className = (rssi === null || rssi === undefined || rssi === 0 || rssi <= THRESHOLD_RSSI_JAVA) ? 'red-bg' : 'green-bg';
            return `<td class="${className}">${rssi || ''}</td>`;
          });

          row.innerHTML = `<td>${drone.deviceName}</td>
                           ${rssiCells[0]}<td>${drone.count[0] || ''}</td>
                           ${rssiCells[1]}<td>${drone.count[1] || ''}</td>
                           ${rssiCells[2]}<td>${drone.count[2] || ''}</td>
                           <td>${drone.isInside ? 'true' : 'false'}</td>`;

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
        <th>RSSI 1</th>
        <th>Counter 1</th>
        <th>RSSI 2</th>
        <th>Counter 2</th>
        <th>RSSI 3</th>
        <th>Counter 3</th>
        <th>isInside</th>
      </tr>
    </thead>
    <tbody id="droneList"></tbody>
  </table>
</body>
</html>
)rawl";

// Funktion zur Initialisierung des WLAN-AP und des Webservers
void setupWiFiAndServer() {

  // AP-Modus starten
  WiFi.softAP(ssid, password);
  Serial.println("Access Point gestartet");

  // Eigene MAC-Adresse anzeigen
  Serial.print("MAC-Adresse: ");
  Serial.println(WiFi.softAPmacAddress());

  // IP-Adresse des Access Points ausgeben
  Serial.print("IP-Adresse: ");
  Serial.println(WiFi.softAPIP());

  // Webserver-Routen
  server.on("/", []() {
    server.send(200, "text/html", htmlPage);
  });

  server.on("/droneData", []() {
    String json = getDroneDataJson(); // Aufruf der Funktion zur Erstellung des JSON
    server.send(200, "application/json", json);
  });

  server.on("/threshold", []() {
  String json;
  StaticJsonDocument<32> doc;
  doc["threshold"] = THRESHOLD_RSSI;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
  });

  server.begin();
  Serial.println("Webserver gestartet");

}


void setup() {
  Serial.begin(115200);
  delay(6000);

  strip.begin(); // NeoPixel initialisieren
  strip.setBrightness(BRIGHTNESS);
  strip.show(); // Sicherstellen, dass die LEDs zu Beginn ausgeschaltet sind

  // WLAN und Webserver einrichten
  setupWiFiAndServer();

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
  server.handleClient(); // Clientanfragen bearbeiten
}

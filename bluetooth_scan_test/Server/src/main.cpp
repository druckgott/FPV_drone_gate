#include <ESP8266WiFi.h>
#include <espnow.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h> // NeoPixel Bibliothek
#include <EEPROM.h> // EEPROM für Speichern von Werten

//info: https://www.digikey.at/en/maker/tutorials/2024/how-to-implement-two-way-communication-in-esp-now
//#define DEBUG // Kommentiere diese Zeile aus, um Debugging-Ausgaben zu deaktivieren

const char* ssid = "LED_GATE_SERVER_01";           // SSID des Access Points
const char* password = "12345678";                  // Passwort des Access Points
int CHANNEL;

const uint8_t senderIDs[][6] = {
  {0x3C, 0x84, 0x27, 0xAF, 0x19, 0x48}, // 3C:84:27:AF:19:48 von Sender 1
  {0xD8, 0x3B, 0xDA, 0x34, 0xAE, 0x20}, // D8:3B:DA:34:AE:20 von Sender 2
  {0xD8, 0x3B, 0xDA, 0x32, 0x0C, 0xC0}, // D8:3B:DA:32:0C:C0 von Sender 3
  // Fügen Sie weitere Sender-MAC-Adressen hinzu
};

int currentSenderIndex = 0;  // Aktueller Sender
unsigned long interval = 200; // Zeitintervall für jeden Sender
unsigned long lastSwitchTime = 0;

#define MAX_DRONES 10  // Maximalanzahl an Drohnen, die gespeichert werden können
#define MAX_VALUES 3   // Maximalanzahl der RSSI und Counter-Werte pro Drohne

//#define THRESHOLD_RSSI -85 // Definierbarer RSSI-Grenzwert für "innerhalb des Bereichs"
int THRESHOLD_RSSI = -85;
int RESETINTERVAL = 50; // Intervall zwischen den Scans in Millisekunden

// NeoPixel Konfiguration
#define NUM_LEDS 150
#define LED_PIN 2          // Pin für die NeoPixel-LEDs (GPIO2, D4)
int BRIGHTNESS = 60; // Helligkeit als Konstante definieren
#define ENABLE_COLOR_PICKER true // Kann auf false gesetzt werden, um den Farbpicker zu deaktivieren

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
bool ledWhenNoDrone = false; // Standardmäßig auf 'false' setzen
unsigned long previousMillis = 0; // speichert die Zeit des letzten Scans
unsigned long elapsedMillis = 0;  // speichert die verstrichene Zeit zwischen Scans
int current_drone_color = 0;

// EEPROM Adressen festlegen
#define EEPROM_SIZE 512                     // Gesamtgröße des EEPROM
#define EEPROM_BRIGHTNESS_ADDR 0
#define EEPROM_THRESHOLD_RSSI_ADDR (EEPROM_BRIGHTNESS_ADDR + sizeof(int))
#define EEPROM_RESET_INTERVAL_ADDR (EEPROM_THRESHOLD_RSSI_ADDR + sizeof(int))
#define EEPROM_COLOR_START_ADDR (EEPROM_RESET_INTERVAL_ADDR + sizeof(int))
#define EEPROM_LED_WHEN_NO_DRONE_ADDR (EEPROM_COLOR_START_ADDR + (10 * sizeof(uint32_t))) // +10 Farben

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
int currentIndex = 0; // Starte mit dem ersten ESP der Daten sendet
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
ESP8266WebServer server(80);

void loadConfigFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE); // Initialisiere EEPROM mit 512 Byte
  EEPROM.get(EEPROM_BRIGHTNESS_ADDR, BRIGHTNESS); // weil negative Werte deswegen get
  EEPROM.get(EEPROM_THRESHOLD_RSSI_ADDR, THRESHOLD_RSSI); // weil negative Werte deswegen get
  
  //Serial.print("Loaded THRESHOLD_RSSI: ");
  //Serial.println(THRESHOLD_RSSI);
  EEPROM.get(EEPROM_RESET_INTERVAL_ADDR, RESETINTERVAL); // Entfernung aus EEPROM laden

  // Lade die Farben aus dem EEPROM
  for (int i = 0; i < 10; i++) {
    int colorAddr = EEPROM_COLOR_START_ADDR + (i * sizeof(uint32_t));
    EEPROM.get(colorAddr, droneColors[i]);
  }

  // Boolean-Wert für LED-Umschaltung laden
  ledWhenNoDrone = EEPROM.read(EEPROM_LED_WHEN_NO_DRONE_ADDR) == 1;
}


void debugEEPROMValues() {
  // Debug-Ausgabe der gewünschten Variablen
  Serial.println("save eprom Debug:");
  Serial.print("LED When No Drone: ");
  Serial.println(ledWhenNoDrone ? "ON" : "OFF");

  Serial.print("Threshold RSSI: ");
  Serial.println(THRESHOLD_RSSI);
  
  Serial.print("Brightness: ");
  Serial.println(BRIGHTNESS);

  Serial.print("ResetInterval: ");
  Serial.println(RESETINTERVAL);
  
  // Farben aus dem EEPROM lesen und ausgeben
  Serial.println("Drone Colors:");
  for (int i = 0; i < 10; i++) {
    Serial.print("Color ");
    Serial.print(i);
    Serial.print(": ");
    Serial.println(droneColors[i], HEX); // Ausgabe im Hex-Format
  }
}

void saveConfigToEEPROM() {
  EEPROM.begin(EEPROM_SIZE); // Initialisiere EEPROM mit 512 Byte
  EEPROM.put(EEPROM_BRIGHTNESS_ADDR, BRIGHTNESS); // Helligkeit speichern
  EEPROM.put(EEPROM_THRESHOLD_RSSI_ADDR, THRESHOLD_RSSI); // THRESHOLD speichern (put weil auch negative Werte)
  EEPROM.put(EEPROM_RESET_INTERVAL_ADDR, RESETINTERVAL); // Entfernung speichern

  // Speichere die Farben in EEPROM
  for (int i = 0; i < 10; i++) {
    int colorAddr = EEPROM_COLOR_START_ADDR + (i * sizeof(uint32_t));
    EEPROM.put(colorAddr, droneColors[i]);
  }

  // Boolean-Wert für LED-Umschaltung speichern
  EEPROM.put(EEPROM_LED_WHEN_NO_DRONE_ADDR, ledWhenNoDrone ? 1 : 0);

  EEPROM.commit(); // Änderungen im EEPROM speichern

   // Debug-Ausgabe
  debugEEPROMValues(); 

}

void printElapsedTime(const String& name) {
  Serial.print("Time Output (");
  Serial.print(name);
  Serial.print("): ");
  Serial.print(elapsedMillis);
  Serial.println(" ms");
}

void setAllLEDs(uint32_t color, uint8_t brightness) {
    strip.setBrightness(brightness); // Setzt die Helligkeit für alle LEDs
    
    for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, color); // Setzt jede LED auf die gleiche Farbe
    }
    
    strip.show(); // Aktualisiert die LEDs, damit die Farbe und Helligkeit angezeigt werden
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

void updateLEDBasedOnRSSI(int closestDroneID, int closestRSSI) {
  if (closestDroneID >= 0 && closestDroneID < 10) {
    // Anzahl der leuchtenden LEDs bestimmen
    int ledCount = 0;
    
    if (closestRSSI >= -30) {
      ledCount = NUM_LEDS; // Alle LEDs leuchten
    } else if (closestRSSI <= -90) {
      ledCount = NUM_LEDS / 2; // 50% der LEDs leuchten
    } else {
      // Interpolation zwischen 0 und -100 für die LED-Anzahl
      ledCount = map(closestRSSI, -90, -30, NUM_LEDS / 2, NUM_LEDS);
    }

    // Setze die Farbe für die LEDs
    for (int i = 0; i < NUM_LEDS; i++) {
      if (i < ledCount) {
        strip.setPixelColor(i, droneColors[closestDroneID]); // Farbe für die aktive Drohne
      }else{
        strip.setPixelColor(i, strip.Color(0, 0, 0)); // Farbe für die aktive Drohne
      }
    }
    
    strip.setBrightness(BRIGHTNESS);
    strip.show(); // Aktualisiere die LEDs
  }
}

void updateDroneStatus(int &closestDroneIndex, float &closestRSSI) {
  closestRSSI = -200;  // Startwert, da RSSI negativ ist
  closestDroneIndex = -1;

  for (int i = 0; i < droneCount; i++) {
    float avgRSSI = 0;
    bool isInside = true;

    // Durchschnitts-RSSI berechnen und prüfen, ob innerhalb des Bereichs
    for (int j = 0; j < MAX_VALUES; j++) {
      avgRSSI += drones[i].rssi[j];

      if (drones[i].rssi[j] < THRESHOLD_RSSI || drones[i].rssi[j] >= 0) {
        isInside = false; // Drohne ist außerhalb des Bereichs
      }
    }

    avgRSSI /= MAX_VALUES; // Durchschnitt berechnen
    drones[i].isInside = isInside;

    // Nächstgelegene Drohne aktualisieren
    if (avgRSSI > closestRSSI && avgRSSI < 0) {
      closestRSSI = avgRSSI;
      closestDroneIndex = i;
    }
  }
}

// Funktion zum Speichern oder Aktualisieren empfangener Daten
void storeDroneData(ReceivedDroneData receivedData) {
  // Extrahiere das Suffix, um den RSSI/Count-Index zu bestimmen
  //printElapsedTime("Start storeDroneData loop");

  int index = -1;
  if (strcmp(&receivedData.deviceName[strlen(receivedData.deviceName) - 3], "_01") == 0) index = 0;
  else if (strcmp(&receivedData.deviceName[strlen(receivedData.deviceName) - 3], "_02") == 0) index = 1;
  else if (strcmp(&receivedData.deviceName[strlen(receivedData.deviceName) - 3], "_03") == 0) index = 2;

  // Wenn das Suffix nicht gefunden wurde, abbrechen
  if (index == -1) return;

  // Überprüfen, ob die Drohne bereits in der Liste ist
  //printElapsedTime("Start drone loop");
  for (int i = 0; i < droneCount; i++) {
    if (strncmp(drones[i].deviceName, receivedData.deviceName, 8) == 0) {
      // Neuen RSSI- und Counter-Wert an der bestimmten Position speichern
      drones[i].rssi[index] = receivedData.rssi;
      drones[i].count[index] = receivedData.count;

      // failed_value aktualisieren, falls der RSSI-Wert den Schwellenwert nicht erreicht
      drones[i].failed_value[index] = (receivedData.rssi < THRESHOLD_RSSI) ? receivedData.rssi : 0;
      //printElapsedTime("Datenempfang beendet");
      // isInside anhand der aktuellen RSSI-Werte neu berechnen
      //drones[i].isInside = calculateIsInside(drones[i].rssi);
      // Aktualisieren der nächstgelegenen Drohne
      int closestDroneIndex;
      float closestRSSI = 31;
      // Aufruf der neuen Funktion
      updateDroneStatus(closestDroneIndex, closestRSSI);
      //printElapsedTime("Update Status beendet");
      //Farbe ändern wenn die näheste Drone innerhalb des Rings ist
      if (drones[i].isInside && closestDroneIndex != -1) {
        current_drone_color = String(drones[closestDroneIndex].deviceName).substring(6).toInt();
      }else if (ledWhenNoDrone) {
        setAllLEDs(strip.Color(255, 255, 255), BRIGHTNESS);
      }
      //printElapsedTime("Update Farbe Led beendet");
      #ifdef DEBUG
      Serial.print("Die nächste Drohne ist: ");
      Serial.println(drones[closestDroneIndex].deviceName);
      #endif
      closest_drone_index = closestDroneIndex;
      closest_RSSI = closestRSSI;
      updateLEDBasedOnRSSI(current_drone_color, closestRSSI);
      //printElapsedTime("Update RSSI Led beendet");
      return; // Aktualisierung vorgenommen, keine weitere Speicherung nötig
    }
  }
  //printElapsedTime("Ende drone loop");
  //printElapsedTime("Start first drone loop");
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
    //drones[droneCount].isInside = calculateIsInside(drones[droneCount].rssi);
    // Aktualisieren der nächstgelegenen Drohne
    int closestDroneIndex;
    float closestRSSI = 31;
    //findClosestDrone(closestDroneIndex, closestRSSI);
    // Aufruf der neuen Funktion
    updateDroneStatus(closestDroneIndex, closestRSSI);
    drones[droneCount].currentIndex = 1; // Setze den aktuellen Index auf 1
    //Farbe ändern wenn die näheste Drone innerhalb des Rings ist
    if (drones[droneCount].isInside && closestDroneIndex != -1) {
      current_drone_color = String(drones[closestDroneIndex].deviceName).substring(6).toInt();
    }else if (ledWhenNoDrone) {
      setAllLEDs(strip.Color(255, 255, 255), BRIGHTNESS);
    }
    #ifdef DEBUG
    Serial.print("Die nächste Drohne ist: ");
    Serial.println(drones[closestDroneIndex].deviceName);
    #endif
    closest_drone_index = closestDroneIndex;
    closest_RSSI = closestRSSI;
    updateLEDBasedOnRSSI(current_drone_color, closestRSSI);

    droneCount++; // Erhöhe die Drohnennummer
  }
  //printElapsedTime("Ende first drone loop");

}

// Callback-Funktion für ESP-NOW-Sendebestätigung
void onDataSent(uint8_t *mac_addr, uint8_t status) {
  //#ifdef DEBUG
  Serial.print("Sende-Status: ");
  Serial.println(status == 0 ? "Erfolgreich" : "Fehlgeschlagen");

  Serial.print("Ziel-MAC-Adresse: ");
  for (int i = 0; i < 6; i++) {
    Serial.print(mac_addr[i], HEX);
    if (i < 5) {
      Serial.print(":");
    }
  }
  Serial.println();
  //#endif
}

// Callback-Funktion für den Empfang von ESP-NOW-Daten
void onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  ReceivedDroneData receivedData;
  memcpy(&receivedData, incomingData, sizeof(receivedData));
  //#ifdef DEBUG
  Serial.print("Daten empfangen von: ");
  for (int i = 0; i < 6; i++) {
      Serial.printf("%02X", mac[i]);
      if (i < 5) Serial.print(":");
  }
  Serial.printf(" | Gerätename: %s | RSSI: %d | Counter: %d\n", receivedData.deviceName, receivedData.rssi, receivedData.count);
  //#endif
  // Drohne zu den Daten hinzufügen oder aktualisieren
  storeDroneData(receivedData);
}

// Funktion zum Erstellen des JSON-Dokuments für die Drohnendaten
String getDroneDataJson() {
  StaticJsonDocument<1024> jsonDoc;
  
  // Verwende jsonDoc["drones"].to<JsonArray>() anstelle von createNestedArray
  JsonArray jsonDrones = jsonDoc["drones"].to<JsonArray>();
  //printElapsedTime("Start webside loop");
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
  //printElapsedTime("Ende webside loop");
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
    .config-button {
      margin-top: 20px; /* Space above */
      padding: 10px 20px; /* Padding for the button */
      font-size: 16px; /* Font size */
      background-color: #4CAF50; /* Green background */
      color: white; /* White text color */
      border: none; /* No borders */
      border-radius: 5px; /* Rounded corners */
      cursor: pointer; /* Pointer on hover */
    }
    .config-button:hover {
      background-color: #45a049; /* Darker green on hover */
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
  <button class="config-button" onclick="window.location.href='/config'">Configuration</button>
  <button class="config-button" target="_blank" onclick="window.location.href='/droneData'">RAW_DATA</button>
</body>
</html>
)rawl";

// Web server handler for the configuration page
void handleConfigPage() {
    String html = R"=====(
    <!DOCTYPE html>
    <html>
    <head>
      <meta charset="UTF-8">
      <title>Configuration</title>
      <style>
        body {
          background-color: #f0f0f0;
          font-family: Arial, sans-serif;
          color: black;
        }
        input[type="number"] {
          width: 50px; /* Width for number fields */
        }
        input[type="text"] {
          width: 80px; /* Width for hex value input fields */
        }
      </style>
      <script>
        function updateHexValue(colorInput, hexInput) {
            document.getElementById(hexInput).value = colorInput.value;
        }
        
        function resetColors() {
            // Reset to default colors
            const defaultColors = [
                "#FF0000", "#FFFF00", "#00FF00", "#0000FF", 
                "#FF00FF", "#00FFFF", "#FFA500", "#800080", 
                "#4bb7d6", "#808080"
            ];
            for (let i = 0; i < 10; i++) {
                document.getElementById("drone_0" + (i + 1)).value = defaultColors[i];
                document.getElementById("drone_0" + (i + 1) + "_hex").value = defaultColors[i];
            }
        }
      </script>
    </head>
    <body>
      <h1>LED Configuration</h1>
      <form action="/saveConfig" method="POST">
        <label for="brightness">LED Brightness (0-255):</label>
        <input type="number" id="brightness" name="brightness" min="0" max="255" value=")=====" + String(BRIGHTNESS) + R"=====(">
        <br><br>

        <label for="thresholdrssi">thresholdrssi Value (-200 bis -1):</label>
        <input type="number" id="thresholdrssi" name="thresholdrssi" min="-200" max="-1" value=")=====" + String(THRESHOLD_RSSI) + R"=====(">
        <br><br>

        <label for="resetInterval">resetInterval Intervall (1-1000ms):</label>
        <input type="number" id="resetInterval" name="resetInterval" min="1" max="1000" value=")=====" + String(RESETINTERVAL) + R"=====(">
        <br><br>

        <label for="ledWhenNoDrone">Switch LED to white when no drone is detected:</label>
        <input type="checkbox" id="ledWhenNoDrone" name="ledWhenNoDrone")=====" + (ledWhenNoDrone ? "checked" : "") + R"=====( value="1">
        <br><br>

        )=====";

#if ENABLE_COLOR_PICKER
    html += "<label for=\"color\">Color selection for drones:</label><br>";

    for (int i = 0; i < 10; i++) {
        String droneName = "drone_0" + String(i + 1);
        String hexColor = String(droneColors[i], HEX);

        // Add leading zeros to ensure the value is 6 digits
        while (hexColor.length() < 6) {
            hexColor = "0" + hexColor; 
        }
        hexColor = "#" + hexColor; // Prepend '#' for the color selection

        html += "<label for=\"" + droneName + "\">Drone_0" + String(i + 1) + " (Colors):</label>";
        html += "<input type=\"color\" id=\"" + droneName + "\" name=\"" + droneName + "\" value=\"" + hexColor + "\" onchange=\"updateHexValue(this, '" + droneName + "_hex')\">";
        html += "<input type=\"text\" id=\"" + droneName + "_hex\" name=\"" + droneName + "_hex\" value=\"" + hexColor + "\"><br>";
        //html += "<span>Current value: " + hexColor + "</span><br>";
    }

    html += "<br><br>";
#endif

    // Button to reset colors
    html += "<input type=\"button\" value=\"Reset colors\" onclick=\"resetColors()\"><br><br>";

    html += R"=====(
        <input type="submit" value="Save">
        <input type="button" value="Back" onclick="window.location.href='/'">
      </form>
    </body>
    </html>
    )=====";

    server.send(200, "text/html", html);
}

// Webserver Handler für das Speichern der Konfiguration
void handleSaveConfig() {
  if (server.hasArg("brightness")) {
    int brightness = server.arg("brightness").toInt();
    if (brightness >= 0 && brightness <= 255) {
      BRIGHTNESS = brightness;
    }
  }

  if (server.hasArg("thresholdrssi")) {
    int thresholdrssi = server.arg("thresholdrssi").toInt();
    if (thresholdrssi >= -200 && thresholdrssi <= -1) {
      THRESHOLD_RSSI = thresholdrssi;
    }
  }

  if (server.hasArg("resetInterval")) {
    int resetInterval = server.arg("resetInterval").toInt();
    if (resetInterval >= 1 && resetInterval <= 1000) {
      RESETINTERVAL = resetInterval;
    }
  }
  
  if (server.hasArg("ledWhenNoDrone")) {
    // Wenn die Checkbox angehakt ist, wird der Wert "1" übergeben, damit ist die variable true gesetzt
    ledWhenNoDrone = (server.arg("ledWhenNoDrone") == "1");
  } else {
    // Wenn das Argument fehlt, ist die Checkbox nicht angehakt, setze ledWhenNoDrone auf false
    ledWhenNoDrone = false;
  }

#if ENABLE_COLOR_PICKER
    for (int i = 0; i < 10; i++) {
        String hexFieldName = "drone_0" + String(i + 1) + "_hex"; // Name des Textfelds für den Hex-Wert
        if (server.hasArg(hexFieldName)) {
            String hexValue = server.arg(hexFieldName); // Hole den Hex-Wert als String

            // Überprüfen, ob das '#' am Anfang steht und nur die letzten 6 Zeichen für RGB verwenden
            if (hexValue.startsWith("#")) {
                hexValue = hexValue.substring(1); // Entferne das '#'
            }

            // Stellen Sie sicher, dass der Hex-Wert 6 Zeichen hat, indem Sie führende Nullen hinzufügen
            while (hexValue.length() < 6) {
                hexValue = "0" + hexValue; // Fügen Sie führende Nullen hinzu
            }

            // Stelle sicher, dass der Hex-Wert 6 Zeichen hat
            if (hexValue.length() == 6) {
                droneColors[i] = strtol(hexValue.c_str(), NULL, 16); // Konvertiere in Ganzzahl
            } else {
                Serial.println("Invalid hex value for Drone " + String(i + 1) + ": " + hexValue); // Error log
            }
            #if DEBUG
            Serial.println("Drone " + String(i + 1) + " Color: " + String(droneColors[i], HEX)); // Debugging
            #endif
        } else {
            Serial.println("No argument found for Drone " + String(i + 1)); // Error log when the argument is missing
        }
    }
#endif

  saveConfigToEEPROM(); // Konfiguration ins EEPROM speichern
  
  // Umleitung zur Konfigurationsseite
  server.sendHeader("Location", "/config"); // Hier "config" sollte der Pfad zur Konfigurationsseite sein
  server.send(303); // HTTP 303 See Other
}

// Funktion zur Initialisierung des WLAN-AP und des Webservers
void setupWiFiAndServer() {

  WiFi.mode(WIFI_AP_STA); // Ermöglicht AP und Station gleichzeitig
  // AP-Modus starten
  WiFi.softAP(ssid, password, CHANNEL);
  Serial.println("Access Point gestartet auf");

  // Eigene MAC-Adresse anzeigen
  Serial.print("MAC-Adresse: ");
  Serial.println(WiFi.softAPmacAddress());

  // IP-Adresse des Access Points ausgeben
  Serial.print("IP-Adresse: ");
  Serial.println(WiFi.softAPIP());

  
  if (int32_t n = WiFi.scanNetworks()) {
      for (uint8_t i=0; i<n; i++) {
          if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
              CHANNEL = WiFi.channel(i); } } }

  wifi_set_channel(CHANNEL);
  Serial.print("CHANNEL: ");
  Serial.println(CHANNEL);

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

  // Neue Routen für Konfigurationsseite
  server.on("/config", handleConfigPage);
  server.on("/saveConfig", HTTP_POST, handleSaveConfig);

  server.begin();
  Serial.println("Webserver gestartet");

}

void initESPNow() {
  if (esp_now_init() != 0) {   // Ersetzen Sie ESP_OK durch 0
    Serial.println("Fehler bei der Initialisierung von ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(onDataRecv);
  esp_now_register_send_cb(onDataSent);
  esp_now_add_peer((uint8_t*)senderIDs[0], ESP_NOW_ROLE_COMBO, CHANNEL, NULL, 0);
  esp_now_add_peer((uint8_t*)senderIDs[1], ESP_NOW_ROLE_COMBO, CHANNEL, NULL, 0);
  esp_now_add_peer((uint8_t*)senderIDs[2], ESP_NOW_ROLE_COMBO, CHANNEL, NULL, 0);
  Serial.println("ESP-NOW erfolgreich initialisiert und bereit für Senden und Empfangen.");
}

void startLed() {
  strip.begin(); // NeoPixel initialisieren
  strip.setBrightness(BRIGHTNESS);
  strip.show(); // Sicherstellen, dass die LEDs zu Beginn ausgeschaltet sind
}

void setup() {
  Serial.begin(115200);
  delay(6000);
  Serial.println("");

  loadConfigFromEEPROM(); // Lade gespeicherte Konfiguration
  startLed();
  // WLAN und Webserver einrichten
  setupWiFiAndServer();
  initESPNow();

  Serial.println("Warte auf ESP-NOW-Nachrichten...");

}

void loop() {
  // ESP-NOW arbeitet mit Callback, daher keine Logik im Haupt-Loop nötig
  server.handleClient(); // Clientanfragen bearbeiten
    // Überprüfen, ob das Intervall abgelaufen ist
  elapsedMillis = millis();
  if ((unsigned long)(elapsedMillis - previousMillis) >= (unsigned long)RESETINTERVAL) {
    // Nächsten Sender bestimmen
    currentSenderIndex = (currentSenderIndex + 1) % (sizeof(senderIDs) / sizeof(senderIDs[0]));
    previousMillis = elapsedMillis;
    
    // Token an aktuellen Sender senden
    // Casten Sie `senderIDs[currentSenderIndex]` und "SEND" zu `u8*`
    char message[] = "SEND"; // Dies ist ein char-Array, das die Größe des Strings enthält
    int result = esp_now_send((uint8_t*)senderIDs[currentSenderIndex], (uint8_t *)message, sizeof(message) - 1); 
    Serial.println("esp_now_send " + String(result));
    Serial.print("Sender ");
    Serial.print(currentSenderIndex);
    Serial.println(" darf jetzt senden.");
  }
}

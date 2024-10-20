#include <EEPROM.h> // EEPROM für Speichern von Werten
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <math.h>  // Für die Logarithmus-Berechnung
#include <ArduinoJson.h> // Für JSON Verarbeitung
#include <Adafruit_NeoPixel.h> // NeoPixel Bibliothek

// Debug-Modus aktivieren oder deaktivieren
//#define DEBUG 1 // Setze auf 1 für aktiv, auf 0 für deaktiviert

// Konfigurationsparameter
unsigned long previousMillis = 0; // speichert die Zeit des letzten Scans
unsigned long elapsedMillis = 0;  // speichert die verstrichene Zeit zwischen Scans
const unsigned long scanInterval = 50; // Intervall zwischen den Scans in Millisekunden
const char* targetSSIDPrefix = "Drone"; // Ersetze "Drone" durch den SSID-Präfix, nach dem gesucht werden soll

// Parameter für die Umrechnung von Entfernung in RSSI
const float A = -40; // Typischer RSSI-Wert in 1 Meter Entfernung
const float n = 2.0; // Signalabfallfaktor für freie Sicht (anpassbar je nach Umgebung)
const char* ssid = "Drone_Gate_01"; // WiFi SSID
const char* password = "12345678"; // WiFi Passwort

// Abstand in Zentimetern (konfigurierbar)
float distance_cm = 150.0; // Setze den gewünschten Abstand in cm

// NeoPixel Konfiguration
#define NUM_LEDS 300
#define LED_PIN 2          // Pin für die NeoPixel-LEDs (GPIO2, D4)
int BRIGHTNESS = 60; // Helligkeit als Konstante definieren
#define ENABLE_COLOR_PICKER true // Kann auf false gesetzt werden, um den Farbpicker zu deaktivieren

// Tiefpassfilter-Konfiguration
int LOW_PASS_FILTER_FREQ = 60;  // Frequenz des Tiefpassfilters in Hz (15 bis 100 Hz)
const float dt = scanInterval / 1000.0; // Zeitintervall in Sekunden
float alpha = 0; // Filterfaktor
float lastFilteredRSSI = 0; // Letzter gefilterter RSSI-Wert
bool ledWhenNoDrone = false; // Standardmäßig auf 'false' setzen

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Webserver auf Port 80
ESP8266WebServer server(80);

// Globale Variablen für Dronendaten
String closestDrone = "";
long closestRSSI = LONG_MIN;
StaticJsonDocument<1024> droneData;

// EEPROM Adressen festlegen
#define EEPROM_BRIGHTNESS_ADDR 0
#define EEPROM_LOW_PASS_FILTER_ADDR 1
#define EEPROM_DISTANCE_CM_ADDR 2
#define EEPROM_COLOR_START_ADDR 10 // Beginn der Farbspeicherung EEPROM_COLOR_START_ADDR + 10 * 4 = 50
#define EEPROM_LED_WHEN_NO_DRONE_ADDR 50 // Adresse für Boolean-Wert

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

// Funktion zur Berechnung des Tiefpassfilters
float lowPassFilter(float newValue) {
    // Berechnung des Filterfaktors alpha
    alpha = dt / (dt + (1.0 / (2 * M_PI * LOW_PASS_FILTER_FREQ)));
    lastFilteredRSSI = alpha * newValue + (1 - alpha) * lastFilteredRSSI; // Filterformel
    return lastFilteredRSSI; // Rückgabe des gefilterten Wertes
}

// Funktion zur Umrechnung von Abstand (in cm) in RSSI
long calculateRSSIFromDistance(float distance_cm) {
    float distance_m = distance_cm / 100.0; // Umrechnung von cm in Meter
    return A - 10 * n * log10(distance_m);  // Berechnung des RSSI-Werts
}

void setAllLEDs(uint32_t color, uint8_t brightness) {
    strip.setBrightness(brightness); // Setzt die Helligkeit für alle LEDs
    
    for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, color); // Setzt jede LED auf die gleiche Farbe
    }
    
    strip.show(); // Aktualisiert die LEDs, damit die Farbe und Helligkeit angezeigt werden
}

// Funktion zur Suche von Dronen in der Umgebung mit einem Abstand in cm
void scanForDrones(float distance_cm) {
    long rssiThreshold = calculateRSSIFromDistance(distance_cm); // Berechnung des RSSI-Schwellenwerts

    // Starten des Scanvorgangs
    int n = WiFi.scanNetworks(false, true, true); // Scan mit aktivem Scan und vollständigen Informationen

    // Zurücksetzen der Dronendaten
    closestDrone = "";
    closestRSSI = LONG_MIN;
    droneData.clear();
    droneData["drones"] = JsonArray(); // JSON-Array für Dronen initialisieren

    // Überprüfen, ob Netzwerke gefunden wurden
    if (n == 0) {
        Serial.println("No networks found.");
        setAllLEDs(strip.Color(255, 255, 255), 20); // aus und 0% wenn kein Netzwerk gefunden
    } else {
        // Durchlaufen aller gefundenen Netzwerke
        for (int i = 0; i < n; i++) {
            String ssid = WiFi.SSID(i);
            long rssi = WiFi.RSSI(i); // RSSI des gefundenen Netzwerks abrufen

            // Gleitenden Durchschnitt berechnen
            float filteredRSSI = lowPassFilter(rssi); // Gefilterten RSSI-Wert berechnen

            if (ssid.startsWith(targetSSIDPrefix)) { // SSID-Präfix prüfen

                #if DEBUG
                Serial.printf("Found: SSID: %s | filtered RSSI: %f\n", ssid.c_str(), filteredRSSI);
                #endif

                // Drone mit dem stärksten RSSI finden
                if (filteredRSSI > closestRSSI) {
                    closestDrone = ssid;
                    closestRSSI = filteredRSSI;
                }

                // Überprüfen, ob die Drone bereits im Array ist
                bool exists = false;
                for (JsonObject droneInfo : droneData["drones"].as<JsonArray>()) {
                    if (droneInfo["ssid"] == ssid) {
                        exists = true;
                        break; // Drone existiert bereits, keine neue hinzufügen
                    }
                }

                // Droneninformationen zur JSON-Datenstruktur hinzufügen
                if (!exists) {
                    JsonObject droneInfo = droneData["drones"].createNestedObject();
                    droneInfo["ssid"] = ssid;
                    droneInfo["rssi"] = filteredRSSI;
                    droneInfo["isClosest"] = (filteredRSSI > calculateRSSIFromDistance(distance_cm)); // Marker für die nächste Drone
                    // Extrahiere die Dronennummer aus der SSID in einer Zeile
                    int droneNumber = (ssid[7] >= '0' && ssid[7] <= '9') ? ssid[7] - '0' : -1; // Für Drone_1 bis Drone_9

                    // Falls die SSID eine zweistellige Zahl hat (Drone_10)
                    if (ssid.length() > 8 && strncmp(ssid.c_str(), "Drone_", 7) == 0 && ssid[8] >= '0' && ssid[8] <= '9') {
                        droneNumber = (ssid[7] - '0') * 10 + (ssid[8] - '0'); // Konvertiere zu einer Zahl
                    }
                    // 1 Abziehen weil die ID bei 1 beginnt, der color array aber bei 0
                    droneInfo["color"] = droneColors[droneNumber - 1];
                }
            }

            size_t droneCount = droneData["drones"].size(); // Größe des JSON-Arrays für Dronen abrufen
            #if DEBUG
            Serial.printf("Number of drones: %zu\n", droneCount); // Output the count
            #endif
            if (droneCount == 0 && ledWhenNoDrone) {
                #if DEBUG
                Serial.println("No Drone_* found.");
                #endif
                setAllLEDs(strip.Color(255, 255, 255), 128); // Weiß und 50% Helligkeit
            }
        }

        // Wenn die nächste Drone gefunden wurde
        #if DEBUG
        if (closestDrone != "") {
            Serial.printf("Nearest drone: SSID: %s | filtered RSSI: %ld\n", closestDrone.c_str(), closestRSSI);
        }
        #endif

        // JSON-Daten aktualisieren
        droneData["closest"]["ssid"] = closestDrone;
        droneData["closest"]["rssi"] = closestRSSI;

        // Farbe der nächstgelegenen Drone auf den NeoPixel setzen
        int closestIndex = closestDrone.substring(closestDrone.indexOf('_') + 1).toInt() - 1; // Drone Nummer aus SSID extrahieren
        if (closestIndex >= 0 && closestIndex < 10) {
            // LEDs basierend auf dem RSSI-Wert steuern
            int ledCount = 0; // Anzahl der leuchtenden LEDs festlegen
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
                    strip.setPixelColor(i, droneColors[closestIndex]); // Farbe für die aktive Drone
                } else {
                    strip.setPixelColor(i, strip.Color(0, 0, 0)); // LEDs ausschalten
                }
            }
            strip.setBrightness(BRIGHTNESS);
            strip.show(); // Aktualisiere die LEDs
        }
    }

    // Leeren des Scanergebnisses
    WiFi.scanDelete();
}

void loadConfigFromEEPROM() {
  EEPROM.begin(512); // Initialisiere EEPROM mit 512 Byte
  BRIGHTNESS = EEPROM.read(EEPROM_BRIGHTNESS_ADDR); // Helligkeit aus EEPROM laden
  LOW_PASS_FILTER_FREQ = EEPROM.read(EEPROM_LOW_PASS_FILTER_ADDR); // Tiefpassfilter aus EEPROM laden
  distance_cm = EEPROM.read(EEPROM_DISTANCE_CM_ADDR); // Entfernung aus EEPROM laden

  // Lade die Farben aus dem EEPROM
  for (int i = 0; i < 10; i++) {
    int colorAddr = EEPROM_COLOR_START_ADDR + (i * sizeof(uint32_t));
    EEPROM.get(colorAddr, droneColors[i]);
  }

  // Boolean-Wert für LED-Umschaltung laden
  ledWhenNoDrone = EEPROM.read(EEPROM_LED_WHEN_NO_DRONE_ADDR) == 1;
}

void saveConfigToEEPROM() {
  EEPROM.write(EEPROM_BRIGHTNESS_ADDR, BRIGHTNESS); // Helligkeit speichern
  EEPROM.write(EEPROM_LOW_PASS_FILTER_ADDR, LOW_PASS_FILTER_FREQ); // Tiefpassfilter speichern
  EEPROM.write(EEPROM_DISTANCE_CM_ADDR, distance_cm); // Entfernung speichern

  // Speichere die Farben in EEPROM
  for (int i = 0; i < 10; i++) {
    int colorAddr = EEPROM_COLOR_START_ADDR + (i * sizeof(uint32_t));
    EEPROM.put(colorAddr, droneColors[i]);
  }

  // Boolean-Wert für LED-Umschaltung speichern
  EEPROM.write(EEPROM_LED_WHEN_NO_DRONE_ADDR, ledWhenNoDrone ? 1 : 0);

  EEPROM.commit(); // Änderungen im EEPROM speichern
}

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

        <label for="lowpass">Lowpass Filter (1-100):</label>
        <input type="number" id="lowpass" name="lowpass" min="1" max="100" value=")=====" + String(LOW_PASS_FILTER_FREQ) + R"=====(">
        <br><br>

        <label for="distance_cm">Distance (1-10000 cm):</label>
        <input type="number" id="distance_cm" name="distance_cm" min="1" max="10000" value=")=====" + String(distance_cm) + R"=====(">
        <span>This is the estimated distance (calculated by RSSI) where the drone will be detected and the color of the gate can be switched.</span>
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

  if (server.hasArg("lowpass")) {
    int lowpass = server.arg("lowpass").toInt();
    if (lowpass >= 1 && lowpass <= 100) {
      LOW_PASS_FILTER_FREQ = lowpass;
    }
  }

  if (server.hasArg("distance_cm")) {
    int distance = server.arg("distance_cm").toInt();
    if (distance >= 1 && distance <= 10000) {
      distance_cm = distance;
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

// Webserver Handler für Dronendaten
void handleDroneData() {
    String response;
    serializeJson(droneData, response);
    server.send(200, "application/json", response);
}

// Webserver Handler for the webpage
void handleRoot() {
    String html = R"=====(
    <!DOCTYPE html>
    <html>
    <head>
      <meta charset="UTF-8">
      <title>Drone Scanner</title>
      <style>
        body {
          background-color: #f0f0f0;
          font-family: Arial, sans-serif;
          color: black;
        }
        table {
          width: 100%;
          border-collapse: collapse;
        }
        table, th, td {
          border: 1px solid black;
        }
        th, td {
          border: 1px solid black;
          padding: 15px;
          text-align: left;
          width: 33.33%; /* Each column takes 1/3 of the total width */
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
        .drone-color {
          width: 100px;
          text-align: center;
          color: white;
        }
      </style>
      <script>
        function fetchDroneData() {
          fetch("/droneData")
            .then(response => response.json())
            .then(data => {
              let droneTable = "";
              // Sort drones by name
              data.drones.sort((a, b) => a.ssid.localeCompare(b.ssid));
              for (let i = 0; i < data.drones.length; i++) {
                let className = data.drones[i].ssid === data.closest.ssid ? "closest" : "farther";
                let hexColor = "#" + ("000000" + data.drones[i].color.toString(16)).slice(-6);
                droneTable += "<tr class='" + className + "'><td>" + data.drones[i].ssid + "</td><td>" + data.drones[i].rssi + "</td><td class='drone-color' style='background-color:" + hexColor + "'>" + hexColor + "</td></tr>";
              }
              document.getElementById("drone-list").innerHTML = droneTable;
            });
        }
        setInterval(fetchDroneData, 100); // Updates data every 0.1 seconds
      </script>
    </head>
    <body>
      <h1>Drone Scanner</h1>
      <table>
        <thead>
          <tr>
            <th>SSID</th>
            <th>RSSI</th>
            <th>Color</th>
          </tr>
        </thead>
        <tbody id="drone-list">
        </tbody>
      </table>
      <button class="config-button" onclick="window.location.href='/config'">Configuration</button>
    </body>
    </html>
    )=====";

    server.send(200, "text/html", html);
}

// Setup-Funktion
void setup() {

    loadConfigFromEEPROM(); // Lade gespeicherte Konfiguration
    Serial.begin(115200);
    WiFi.mode(WIFI_AP); // AP-Mode aktivieren
    WiFi.softAP(ssid, password); // SSID und Passwort setzen
    IPAddress IP = WiFi.softAPIP();      // IP-Adresse im AP-Modus holen
    Serial.println("WiFi AP started with IP: ");
    Serial.println(IP);

    strip.begin(); // NeoPixel initialisieren
    strip.setBrightness(BRIGHTNESS);
    strip.show(); // Sicherstellen, dass die LEDs zu Beginn ausgeschaltet sind

    server.on("/", handleRoot); // Root-Handler
    server.on("/droneData", handleDroneData); // JSON-Handler
    
    // Neue Routen für Konfigurationsseite
    server.on("/config", handleConfigPage);
    server.on("/saveConfig", HTTP_POST, handleSaveConfig);
    
    server.begin(); // Webserver starten
    Serial.println("Web server started.");

}

// Loop-Funktion
void loop() {
    server.handleClient(); // Überprüfen auf eingehende Anfragen
    elapsedMillis = millis();
    if (elapsedMillis - previousMillis >= scanInterval) {
        previousMillis = elapsedMillis;
        scanForDrones(distance_cm); // Scannen nach Dronen
    }
}

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <math.h>  // Für die Logarithmus-Berechnung
#include <ArduinoJson.h> // Für JSON Verarbeitung
#include <Adafruit_NeoPixel.h> // NeoPixel Bibliothek

// Konfigurationsparameter
unsigned long previousMillis = 0; // speichert die Zeit des letzten Scans
unsigned long elapsedMillis = 0;  // speichert die verstrichene Zeit zwischen Scans
const unsigned long scanInterval = 50; // Intervall zwischen den Scans in Millisekunden
const char* targetSSIDPrefix = "Drohne"; // Ersetze "Drohne" durch den SSID-Präfix, nach dem gesucht werden soll

// Parameter für die Umrechnung von Entfernung in RSSI
const float A = -40; // Typischer RSSI-Wert in 1 Meter Entfernung
const float n = 2.0; // Signalabfallfaktor für freie Sicht (anpassbar je nach Umgebung)
const char* ssid = "Drone_Gate_01"; // WiFi SSID
const char* password = "12345678"; // WiFi Passwort

// Abstand in Zentimetern (konfigurierbar)
const float distance_cm = 150.0; // Setze den gewünschten Abstand in cm

// NeoPixel Konfiguration
#define NUM_LEDS 300
#define LED_PIN 2          // Pin für die NeoPixel-LEDs (GPIO2, D4)
#define BRIGHTNESS 60 // Helligkeit als Konstante definieren

// Tiefpassfilter-Konfiguration
#define LOW_PASS_FILTER_FREQ 60  // Frequenz des Tiefpassfilters in Hz (15 bis 100 Hz)
const float dt = scanInterval / 1000.0; // Zeitintervall in Sekunden
float alpha = 0; // Filterfaktor
float lastFilteredRSSI = 0; // Letzter gefilterter RSSI-Wert

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Webserver auf Port 80
ESP8266WebServer server(80);

// Globale Variablen für Drohnendaten
String closestDrone = "";
long closestRSSI = LONG_MIN;
StaticJsonDocument<1024> droneData;

// Farben für jede Drohne
const uint32_t droneColors[] = {
    0xFF0000, // Drone_01: Rot
    0xFFFF00, // Drone_02: Gelb
    0x00FF00, // Drone_03: Grün
    0x0000FF, // Drone_04: Blau
    0xFF00FF, // Drone_05: Magenta
    0x00FFFF, // Drone_06: Cyan
    0xFFA500, // Drone_07: Orange
    0x800080, // Drone_08: Lila
    0xFFFFFF, // Drone_09: Weiß
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

// Funktion zur Suche von Drohnen in der Umgebung mit einem Abstand in cm
void scanForDrones(float distance_cm) {
    long rssiThreshold = calculateRSSIFromDistance(distance_cm); // Berechnung des RSSI-Schwellenwerts

    // Starten des Scanvorgangs
    int n = WiFi.scanNetworks(false, true, true); // Scan mit aktivem Scan und vollständigen Informationen

    // Zurücksetzen der Drohnendaten
    closestDrone = "";
    closestRSSI = LONG_MIN;
    droneData.clear();
    droneData["drones"] = JsonArray(); // JSON-Array für Drohnen initialisieren

    // Überprüfen, ob Netzwerke gefunden wurden
    if (n == 0) {
        Serial.println("Keine Netzwerke gefunden.");
    } else {
        // Durchlaufen aller gefundenen Netzwerke
        for (int i = 0; i < n; i++) {
            String ssid = WiFi.SSID(i);
            long rssi = WiFi.RSSI(i); // RSSI des gefundenen Netzwerks abrufen

            // Gleitenden Durchschnitt berechnen
            float filteredRSSI = lowPassFilter(rssi); // Gefilterten RSSI-Wert berechnen

            if (ssid.startsWith(targetSSIDPrefix)) { // SSID-Präfix prüfen
                Serial.printf("Gefunden: SSID: %s | gefilterter RSSI: %f\n", ssid.c_str(), filteredRSSI);

                // Drohne mit dem stärksten RSSI finden
                if (filteredRSSI > closestRSSI) {
                    closestDrone = ssid;
                    closestRSSI = filteredRSSI;
                }

                // Überprüfen, ob die Drohne bereits im Array ist
                bool exists = false;
                for (JsonObject droneInfo : droneData["drones"].as<JsonArray>()) {
                    if (droneInfo["ssid"] == ssid) {
                        exists = true;
                        break; // Drohne existiert bereits, keine neue hinzufügen
                    }
                }

                // Drohneninformationen zur JSON-Datenstruktur hinzufügen
                if (!exists) {
                    JsonObject droneInfo = droneData["drones"].createNestedObject();
                    droneInfo["ssid"] = ssid;
                    droneInfo["rssi"] = filteredRSSI;
                    droneInfo["isClosest"] = (filteredRSSI > calculateRSSIFromDistance(distance_cm)); // Marker für die nächste Drohne
                }
            }
        }

        // Wenn die nächste Drohne gefunden wurde
        if (closestDrone != "") {
            Serial.printf("Nächste Drohne: SSID: %s | gefilterter RSSI: %ld\n", closestDrone.c_str(), closestRSSI);
        }

        // JSON-Daten aktualisieren
        droneData["closest"]["ssid"] = closestDrone;
        droneData["closest"]["rssi"] = closestRSSI;

        // Farbe der nächstgelegenen Drohne auf den NeoPixel setzen
        int closestIndex = closestDrone.substring(closestDrone.indexOf('_') + 1).toInt() - 1; // Drohne Nummer aus SSID extrahieren
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
                    strip.setPixelColor(i, droneColors[closestIndex]); // Farbe für die aktive Drohne
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

// Webserver Handler für Drohnendaten
void handleDroneData() {
    String response;
    serializeJson(droneData, response);
    server.send(200, "application/json", response);
}

// Webserver Handler für die Webseite
void handleRoot() {
    String html = R"=====(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Drohnen Scanner</title>
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
          padding: 15px;
          text-align: left;
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
        function fetchDroneData() {
          fetch("/droneData")
            .then(response => response.json())
            .then(data => {
              let droneTable = "";
              // Sortieren der Drohnen nach Namen
              data.drones.sort((a, b) => a.ssid.localeCompare(b.ssid));
              for (let i = 0; i < data.drones.length; i++) {
                let className = data.drones[i].ssid === data.closest.ssid ? "closest" : "farther";
                droneTable += "<tr class='" + className + "'><td>" + data.drones[i].ssid + "</td><td>" + data.drones[i].rssi + "</td></tr>";
              }
              document.getElementById("drone-list").innerHTML = droneTable;
            });
        }
        setInterval(fetchDroneData, 100); // Aktualisiert die Daten alle 0.1 Sekunde
      </script>
    </head>
    <body>
      <h1>Drohnen Scanner</h1>
      <table>
        <thead>
          <tr>
            <th>SSID</th>
            <th>RSSI</th>
          </tr>
        </thead>
        <tbody id="drone-list">
        </tbody>
      </table>
    </body>
    </html>
    )=====";

    server.send(200, "text/html", html);
}

// Setup-Funktion
void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_AP); // AP-Mode aktivieren
    WiFi.softAP(ssid, password); // SSID und Passwort setzen
    Serial.println("WiFi AP gestartet.");

    strip.begin(); // NeoPixel initialisieren
    strip.setBrightness(BRIGHTNESS);
    strip.show(); // Sicherstellen, dass die LEDs zu Beginn ausgeschaltet sind

    server.on("/", handleRoot); // Root-Handler
    server.on("/droneData", handleDroneData); // JSON-Handler
    server.begin(); // Webserver starten
    Serial.println("Webserver gestartet.");
}

// Loop-Funktion
void loop() {
    server.handleClient(); // Überprüfen auf eingehende Anfragen
    elapsedMillis = millis();
    if (elapsedMillis - previousMillis >= scanInterval) {
        previousMillis = elapsedMillis;
        scanForDrones(distance_cm); // Scannen nach Drohnen
    }
}

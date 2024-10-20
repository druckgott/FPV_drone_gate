#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>

ESP8266WebServer server(80);

// Standardwerte für SSID und Passwort
String baseSSID = "Drone_";
String ssidSuffix = "01";  // Suffix der SSID aus EEPROM oder Default
const char* password = "";  // Passwort bleibt leer für ungesichertes WLAN

// EEPROM-Speicherbereich für den SSID-Suffix
const int ssidSuffixAddress = 0; // Speicheradresse für den SSID-Suffix
const int ssidSuffixSize = 2; // Größe des Speicherbereichs für den SSID-Suffix (z.B. 01, 02, ..., 10)

void setup() {
  Serial.begin(115200);

  // EEPROM initialisieren (benötigt 512 Bytes)
  EEPROM.begin(512);

  // SSID-Suffix aus dem EEPROM laden (Adresse 0)
  int storedSuffix = EEPROM.read(ssidSuffixAddress);
  if (storedSuffix >= 1 && storedSuffix <= 10) {
    ssidSuffix = String(storedSuffix < 10 ? "0" : "") + String(storedSuffix);
  } else {
    EEPROM.write(ssidSuffixAddress, 1);  // Standard auf "01" setzen, falls nichts gültiges gespeichert ist
    EEPROM.commit();
  }

  // WLAN Access Point mit der aktualisierten SSID starten
  String fullSSID = baseSSID + ssidSuffix;
  WiFi.softAP(fullSSID.c_str(), password, 1);

  Serial.println();
  Serial.println("WiFi Access Point started:");
  Serial.print("SSID: ");
  Serial.println(fullSSID);
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());

  // Konfigurationsseite einrichten
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.begin();
}

void loop() {
  server.handleClient();  // Webserver-Client-Verbindungen verwalten
}

// HTML Configuration Page
void handleRoot() {
  String html = R"(
    <html>
      <head>
      <meta charset="UTF-8">
      <title>SSID Configuration</title>
      </head>
      <body>
        <h1>SSID Configuration</h1>
        <form action="/save" method="POST">
          <label for="ssidSuffix">SSID Suffix (01-10):</label>
          <select id="ssidSuffix" name="ssidSuffix">
  )";

  for (int i = 1; i <= 10; i++) {
    String optionValue = String(i < 10 ? "0" : "") + String(i);
    html += "<option value='" + String(i) + "'";
    if (ssidSuffix == optionValue) {
      html += " selected";
    }
    html += ">" + optionValue + "</option>";
  }

  html += R"(
          </select>
          <input type="submit" value="Save">
        </form>
      </body>
    </html>
  )";

  server.send(200, "text/html", html);
}

// Save SSID suffix and restart the ESP
void handleSave() {
  if (server.hasArg("ssidSuffix")) {
    int newSuffix = server.arg("ssidSuffix").toInt();
    if (newSuffix >= 1 && newSuffix <= 10) {
      EEPROM.write(ssidSuffixAddress, newSuffix);
      EEPROM.commit();
      server.send(200, "text/html", "<html><body><h1>SSID saved! Restarting...</h1></body></html>");
      delay(1000);  // Small delay before restarting the ESP
      ESP.restart();  // Restart ESP
    } else {
      server.send(200, "text/html", "<html><body><h1>Invalid value!</h1></body></html>");
    }
  } else {
    server.send(200, "text/html", "<html><body><h1>Error: No data received!</h1></body></html>");
  }
}

#include <ESP8266WiFi.h>

const char* ssid = "Drohne_02"; // SSID des WLANs
const char* password = "";   // Passwort, falls erforderlich (leer für ungesichert)

void setup() {
  // Starte die serielle Kommunikation
  Serial.begin(115200);
  
  // Konfiguriere den WLAN-Access Point
  WiFi.softAP(ssid, password, 1);

  Serial.println();
  Serial.println("WiFi Access Point gestartet:");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("IP-Adresse: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  // Hier kann zusätzlicher Code hinzugefügt werden
}

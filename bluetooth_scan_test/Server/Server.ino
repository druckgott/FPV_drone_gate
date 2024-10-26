#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "LED_GATE_SERVER_01";           // SSID des Access Points
const char* password = "12345678";                  // Passwort des Access Points
WiFiUDP udp;
const int udpPort = 1234;                           // Port, auf dem dieser ESP8266 hört

void setup() {
    Serial.begin(115200);
    delay(3000); // 1 Sekunde warten
    // AP-Modus starten
    WiFi.softAP(ssid, password);
    Serial.println("Access Point gestartet");
    
    udp.begin(udpPort);
    Serial.println("Warte auf UDP-Nachrichten...");
}

void loop() {
    int packetSize = udp.parsePacket();
    if (packetSize) {
        uint8_t buffer[6]; // Erwartete Nachricht mit 6 Bytes
        int len = udp.read(buffer, sizeof(buffer));
        
        if (len == sizeof(buffer)) {
            // Entschlüsselung der Nachricht
            String deviceName = "Drone_";
            deviceName += String(buffer[1]) + String(buffer[2]); // Drohnen-ID (zwei Ziffern)
            int rssi = (int)buffer[3] - 200; // RSSI um 200 zurücksetzen

            // Ausgabe der entschlüsselten Informationen
            Serial.printf("Empfangen von %s: RSSI: %d\n", deviceName.c_str(), rssi);
        } else {
            Serial.println("Fehler: Unerwartete Paketgröße");
        }
    }
    delay(10); // Kurze Pause, um die CPU zu entlasten
}

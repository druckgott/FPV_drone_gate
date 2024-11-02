/*
  Flow of the code

  1 - Put WiFi in STA Mode
  2 - Intialize ESPNOW
  3 - Set Role to Combo
  4 - Add peer device
  5 - Define Send Callback Function
  6 - Define Receive Callback Function
*/

#include <ESP8266WiFi.h>
#include <espnow.h>
int CHANNEL = 1;
#define UPDATE_DELAY 2500
unsigned long lastUpdateMillis = 0UL;

const char* ssid = "LED_GATE_SERVER_01";           // SSID des Access Points
const char* password = "12345678";                  // Passwort des Access Points

// REPLACE WITH THE MAC Address of your receiver
uint8_t broadcastAddress[] = {0x3C, 0x84, 0x27, 0xAF, 0x19, 0x48}; // ESP32 MAC
//Structure example to send data
//Must match the receiver structure
typedef struct struct_message {
  char msg[50];
} struct_message;

// Create a struct_message called outgoingReadings to hold outgoing data
struct_message outgoingReadings;

// Create a struct_message called incomingReadings to hold incoming data
struct_message incomingReadings;

// Variable to store if sending data was successful
String success;

// Callback when data is sent
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("Last Packet Send Status: "); 
  if (sendStatus == 0) {
    Serial.println("Delivery success");
  }
  else {
    Serial.println("Delivery fail");
  }
}

// Callback when data is received
void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("Data: "); Serial.println(incomingReadings.msg);
}


void setup() {
  // Init Serial Monitor
  Serial.begin(115200);
  Serial.println("Code start");                             

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_AP_STA); // Ermöglicht AP und Station gleichzeitig
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  
  // AP-Modus starten
  WiFi.softAP(ssid, password);

  // MAC-Adresse ausgeben
  Serial.print("MAC-Adresse: ");
  Serial.println(WiFi.softAPmacAddress());

  // IP-Adresse ausgeben
  Serial.print("IP-Adresse: ");
  Serial.println(WiFi.softAPIP());

  // WLAN-Kanal ausgeben
  Serial.print("WLAN-Kanal: ");
  Serial.println(WiFi.channel());

  // Init ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Set ESP-NOW Role
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);

  // Register peer
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_COMBO, CHANNEL, NULL, 0);

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);

  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {

  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdateMillis >= UPDATE_DELAY) {
    strcpy(outgoingReadings.msg, "Hello from NodeMCU");
    esp_now_send(broadcastAddress, (uint8_t *) &outgoingReadings, sizeof(outgoingReadings));
    lastUpdateMillis = currentMillis;
  }
}

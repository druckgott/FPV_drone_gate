#include <stdio.h> // Einbinden der Standard-I/O-Bibliothek für printf-Funktion
#include "esp_log.h" // Einbinden der ESP-IDF Log-Bibliothek für Logging
#include "esp_bt.h" // Einbinden der ESP-IDF Bluetooth-Bibliothek
#include "esp_gap_ble_api.h" // Einbinden der API für BLE-GAP (Generic Access Profile)
#include "esp_bt_main.h" // Einbinden der Haupt-Bluetooth-API

static const char* TAG = "BLE_Scanner"; // Tag für Log-Nachrichten zur Identifizierung des Moduls

    // Konfiguration der Scan-Parameter
    esp_ble_scan_params_t scan_params = {
        .scan_type              = BLE_SCAN_TYPE_ACTIVE, // Aktiver Scan, bei dem der Scanner nach Geräten sucht und eine Anfrage sendet
        .own_addr_type          = BLE_ADDR_TYPE_PUBLIC, // Öffentliche Adresse für den Scanner
        .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL, // Erlaube alle gefundenen Geräte
        .scan_interval          = 0x50, // Scan-Intervall in Einheiten von 0,625 ms (0x50 = 80 * 0,625 ms = 50 ms)
        .scan_window            = 0x49  // Scan-Fenster in Einheiten von 0,625 ms (0x30 = 48 * 0,625 ms = 30 ms)
    };

// Callback für BLE-Events, wird aufgerufen, wenn BLE-Ereignisse auftreten
void gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    switch (event) {
        case ESP_GAP_BLE_SCAN_RESULT_EVT: { // Event, das auftritt, wenn ein Scan-Ergebnis empfangen wird
            esp_ble_gap_cb_param_t* scan_result = param; // Cast der Parameter zu den BLE-Scan-Ergebnissen
            
            // Prüfen, ob ein BLE-Gerät gefunden wurde
            if (scan_result->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
                // Ausgabe der MAC-Adresse des gefundenen Geräts
                ESP_LOGI(TAG, "Found device with MAC:"); // Log-Nachricht zur Anzeige, dass ein Gerät gefunden wurde
                for (int i = 0; i < 6; i++) { // Schleife durch die 6 Bytes der MAC-Adresse
                    printf("%02X", scan_result->scan_rst.bda[i]); // Ausgabe jedes Bytes in hexadezimaler Form
                    if (i < 5) printf(":"); // Füge einen Doppelpunkt zwischen den Bytes hinzu, außer nach dem letzten Byte
                }
                printf("\n"); // Zeilenumbruch nach der Ausgabe der MAC-Adresse

                // Überprüfen, ob der Name des Geräts in den Advertisements vorhanden ist
                if (scan_result->scan_rst.adv_data_len > 0) { // Prüfen, ob Werbedaten vorhanden sind
                    uint8_t* adv_data = scan_result->scan_rst.ble_adv; // Zugriff auf die Werbedaten
                    uint8_t len = adv_data[0]; // Länge der Werbedaten aus dem ersten Byte
                    uint8_t type = adv_data[1]; // Typ der Werbedaten aus dem zweiten Byte

                    // Prüfen, ob der Typ der Werbedaten den Gerätenamen anzeigt (0x09)
                    if (type == 0x09) { // Typ 0x09 zeigt den Gerätenamen an
                        printf("Device Name: "); // Ausgabe der Meldung "Gerätename:"
                        for (int i = 2; i < len + 1; i++) { // Schleife durch die Zeichen des Gerätenamens
                            printf("%c", adv_data[i]); // Ausgabe jedes Zeichens des Gerätenamens
                        }
                        printf("\n"); // Zeilenumbruch nach dem Gerätenamen
                    }
                }
            }
            break; // Beende den Fall für das Scan-Ergebnis
        }
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: // Event, das auftritt, wenn die Scan-Parameter gesetzt wurden
            ESP_LOGI(TAG, "Starting BLE scan..."); // Log-Nachricht, dass der BLE-Scan startet
            esp_ble_gap_start_scanning(0); // Starte den BLE-Scan für 10 Sekunden
            break; // Beende den Fall für das Setzen der Scan-Parameter

        default: // Standardfall für nicht definierte Ereignisse
            break; // Nichts zu tun
    }
}

// Hauptfunktion des Programms
void app_main() {
    // Initialisiere Bluetooth
    esp_err_t ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT); // Freigabe des Speichers für klassischen BT
    if (ret) { // Überprüfen auf Fehler
        ESP_LOGE(TAG, "Failed to release BT memory: %s", esp_err_to_name(ret)); // Log-Fehler, wenn die Speicherfreigabe fehlschlägt
        return; // Beende die Funktion bei Fehler
    }

    // Konfiguration des Bluetooth-Controllers
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT(); // Standardkonfiguration für den BT-Controller
    ret = esp_bt_controller_init(&bt_cfg); // Initialisierung des Bluetooth-Controllers mit der Konfiguration
    if (ret) { // Überprüfen auf Fehler
        ESP_LOGE(TAG, "Bluetooth controller initialize failed: %s", esp_err_to_name(ret)); // Log-Fehler
        return; // Beende die Funktion bei Fehler
    }

    // Aktiviere den Bluetooth-Controller im BLE-Modus
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE); // Aktivierung des Bluetooth-Controllers im BLE-Modus
    if (ret) { // Überprüfen auf Fehler
        ESP_LOGE(TAG, "Bluetooth controller enable failed: %s", esp_err_to_name(ret)); // Log-Fehler
        return; // Beende die Funktion bei Fehler
    }

    // Initialisiere den Bluedroid-Stack für BLE
    ret = esp_bluedroid_init(); // Initialisierung des Bluedroid-Stacks
    if (ret) { // Überprüfen auf Fehler
        ESP_LOGE(TAG, "Bluedroid stack initialization failed: %s", esp_err_to_name(ret)); // Log-Fehler
        return; // Beende die Funktion bei Fehler
    }

    ret = esp_bluedroid_enable(); // Aktivierung des Bluedroid-Stacks
    if (ret) { // Überprüfen auf Fehler
        ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret)); // Log-Fehler
        return; // Beende die Funktion bei Fehler
    }

    // Setze Scan-Parameter für BLE
    esp_ble_gap_register_callback(gap_callback); // Registriere den Callback für BLE-GAP-Ereignisse


    // Setze die Scan-Parameter
    esp_ble_gap_set_scan_params(&scan_params); // Setze die Scan-Parameter für den BLE-GAP
}

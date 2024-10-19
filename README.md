![config screenshot](ledgate/img/Drone_Gate.png)

![config screenshot](ledgate/img/Config.png)

# LED-Anschluss: 
* Am FPV Drone Gate können über den D4 Pin (GPIO2) LEDs angeschlossen werden.

# Hotspot-Scan: 
* Das Gate scannt nach anderen Hotspots mit den Namen Drone_XX, bis maximal 10 Drones gefunden werden.

# ESP8266 in Drones: 
* Auf jeder Drone kann ein ESP8266 verbaut sein. Dieser wird gescannt, ohne eine Verbindung aufzubauen, um den Ablauf zu beschleunigen.

# LED-Farbauswahl: 
* Für jede Drone kann eine individuelle LED-Farbe ausgewählt werden. Basierend auf dem RSSI-Signal wird die LED-Farbe des Gates auf die Farbe der Drone eingestellt, die den niedrigsten RSSI-Wert hat.

# Zusätzliche Konfigurationen: 
* Es sind einige weitere grundlegende Konfigurationen möglich, um die Benutzererfahrung zu optimieren.

# Aufbau des Gates:
* Fußbodenheitungsrohr mit LED Streifen breite 5 mm WS2812B LED´s, 60Leds pro Meter
* 3D Druck Aufnahme
* ESP-Wroom-02 mit 18650 Zelle
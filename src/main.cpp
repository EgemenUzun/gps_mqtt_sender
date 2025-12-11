#include <Arduino.h>
#include "nmea_parser.hpp"
#include <ArduinoJson.h>
#include <WiFiMulti.h>

#define WIFI_SSID "WIFI"
#define WIFI_PASSWORD "Password"

String gps_line = "";
NMEA_GPRMC_t rmc;
NMEA_GGA_t gga;
WiFiMulti wifi;
bool isConnected = false;

void read_from_gps();
void handle_wifi_connection();

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  pinMode(LED_BUILTIN, OUTPUT); // blue diode
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
  handle_wifi_connection();
  read_from_gps();
}

void read_from_gps() {
  while (Serial2.available() > 0) {
    char c = Serial2.read();

    if (c == '\n') {
      gps_line.trim();

      if (gps_line.startsWith("$GPRMC")) {
        NMEA_GPRMC_t rmc;
        if (parse_gprmc(gps_line.c_str(), &rmc)){
          // Serial.printf("Lat: %.6f, Lon: %.6f, Timestamp: %.2f\n", rmc.latitude, rmc.longitude, rmc.time_utc);
          serializeJson(serialize_gprmc(&rmc), Serial);
          Serial.println();
        }
      } else if (gps_line.startsWith("$GPGGA")) {
        NMEA_GGA_t gga;
        if (parse_gpgga(gps_line.c_str(), &gga)) {
          // Serial.printf("Sat: %d, Alt: %.2f Lon: %.2f, Timestamp: %.2f\n", gga.num_satellites, gga.altitude_msl, gga.time_utc);
          serializeJson(serialize_gpgga(&gga), Serial);
          Serial.println();
        }
      }

      gps_line = "";
    } else {
      gps_line += c;
    }
  }
}

void handle_wifi_connection() {
  if (WiFi.status() == WL_CONNECTED && !isConnected) {
    Serial.println("Connected");
    digitalWrite(LED_BUILTIN, HIGH);
    isConnected = true;
  }

  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    isConnected = false;
    delay(500);
  }
}
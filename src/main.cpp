#include <Arduino.h>
#include "nmea_parser.hpp"
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "WiFi.h"
#include "secrets.h"
#include <PubSubClient.h>

#define AWS_IOT_PUBLISH_TOPIC   "esp32/pub"

WiFiClientSecure net;
PubSubClient client(net);

String gps_line = "";
NMEA_GPRMC_t rmc;
NMEA_GGA_t gga;
bool isWifiConnected = false;

void read_from_gps();
void handle_wifi_connection();
void connectAWS();
void publish_gps_rmc(NMEA_GPRMC_t* rmc);
void publish_gps_gga(NMEA_GGA_t* gga);
void messageHandler(char* topic, byte* payload, unsigned int length);


void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  pinMode(LED_BUILTIN, OUTPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  client.setServer(AWS_IOT_ENDPOINT, 8883);
}

void loop() {
  try {
    handle_wifi_connection();

    if (!isWifiConnected) return;

    if (!client.connected()) {
      connectAWS();
    }

    client.loop();
    read_from_gps();
  } catch(const std::exception& e){
    Serial.println(e.what());
    isWifiConnected = false;
  }
}


void read_from_gps() {
  while (Serial2.available() > 0) {
    char c = Serial2.read();

    if (c == '\n') {
      gps_line.trim();

      if (gps_line.startsWith("$GPRMC")) {
        NMEA_GPRMC_t rmc;
        if (parse_gprmc(gps_line.c_str(), &rmc)){
          publish_gps_rmc(&rmc);
        }
      }  
      else if (gps_line.startsWith("$GPGGA")) {
        NMEA_GGA_t gga;
        if (parse_gpgga(gps_line.c_str(), &gga)) {
          publish_gps_gga(&gga);
        }
      }

      gps_line = "";
    } 
    else {
      gps_line += c;
    }
  }
}

void handle_wifi_connection() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!isWifiConnected) {
      Serial.println("WiFi Connected!");
      digitalWrite(LED_BUILTIN, HIGH);
      isWifiConnected = true;
    }
  } 
  else {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

    if (isWifiConnected) {
      Serial.println("WiFi Lost! Reconnecting...");
      isWifiConnected = false;
    }

    static unsigned long lastAttempt = 0;
    if (millis() - lastAttempt > 3000) {
      lastAttempt = millis();
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      Serial.println("Trying WiFi reconnect...");
    }

    delay(200);
  }
}


void connectAWS() {
  if (!isWifiConnected) return;

  Serial.print("Connecting to AWS IoT...");

  while (!client.connected()) {
    if (client.connect(THINGNAME)) {
      Serial.println(" Connected!");
    } else {
      Serial.print(".");
      delay(200);
    }
  }
}


void publish_gps_rmc(NMEA_GPRMC_t* rmc) {
  if (!client.connected()) return;

  char buffer[256];
  serializeJson(serialize_gprmc(rmc), buffer);

  client.publish(AWS_IOT_PUBLISH_TOPIC, buffer);
}

void publish_gps_gga(NMEA_GGA_t* gga) {
  if (!client.connected()) return;

  char buffer[256];
  serializeJson(serialize_gpgga(gga), buffer);

  client.publish(AWS_IOT_PUBLISH_TOPIC, buffer);
}
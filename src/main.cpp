#include <Arduino.h>
#include "neo6m_gps.hpp"
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "WiFi.h"
#include "secrets.h"
#include <PubSubClient.h>
#include <time.h>

#define AWS_IOT_PUBLISH_TOPIC   "esp32/pub"

WiFiClientSecure net;
PubSubClient client(net);

String gps_line = "";
NMEA_GPRMC_t rmc;
NMEA_GGA_t gga;
bool isWifiConnected = false;
volatile uint8_t gps_data_ready = 0;
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 0;
bool isTimeConfigured = false;

void read_from_gps();
void handle_wifi_connection();
void connectAWS();
void publish_gps_rmc(NMEA_GPRMC_t* rmc);
void publish_gps_gga(NMEA_GGA_t* gga);
void publish_gps(NMEA_GGA_t* gga, NMEA_GPRMC_t* rmc);
void messageHandler(char* topic, byte* payload, unsigned int length);
void setupTime();


void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  pinMode(LED_BUILTIN, OUTPUT);

  configure_neo6m(Serial2);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  client.setServer(AWS_IOT_ENDPOINT, 8883);
}

void loop() {
  try {
    handle_wifi_connection();
    read_from_gps();

    if (!isWifiConnected) return;

    if (isWifiConnected && !isTimeConfigured) {
      setupTime();
    }

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

      if (gps_data_ready == 2) {
        publish_gps(&gga, &rmc);
        // serializeJson(serialize_gps(&gga, &rmc), Serial);
        gps_data_ready = 0;
      }

      if (gps_line.startsWith("$GPRMC")) {
        NMEA_GPRMC_t rmc;
        if (parse_gprmc(gps_line.c_str(), &rmc)){
          gps_data_ready++;
        }
      } else if (gps_line.startsWith("$GPGGA")) {
        NMEA_GGA_t gga;
        if (parse_gpgga(gps_line.c_str(), &gga)) {
          gps_data_ready++;
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

void publish_gps(NMEA_GGA_t* gga, NMEA_GPRMC_t* rmc) {
  if (!client.connected()) return;

  char buffer[256];
  JsonDocument doc = serialize_gps(gga, rmc);
  uint64_t utcMillis = ((uint64_t)time(NULL)) * 1000 + (esp_timer_get_time() % 1000000) / 1000;
  doc["timestamp"] = utcMillis;
  serializeJson(doc, buffer);

  client.publish(AWS_IOT_PUBLISH_TOPIC, buffer);
}

void setupTime() {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    struct tm timeinfo;
    while (!getLocalTime(&timeinfo)) {
        Serial.println("Waiting for NTP time...");
        delay(1000);
    }
    Serial.println("Time synchronized");
    isTimeConfigured = true;
}
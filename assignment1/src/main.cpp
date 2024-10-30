#include <Arduino.h>
#include <WiFi.h>
#include "ThingSpeak.h"
#include <DHT.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Daikin.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <AsyncTCP.h> // https://randomnerdtutorials.com/esp32-esp8266-web-server-http-authentication/
#include <base64.h>
#include <CustomJWT.h>
#include <ESPmDNS.h>
#include <vector>
#include <web.h>
#include <data.h>
#include <config.h>
#include <numeric>

#define LEDPIN 2
#define IRTXPIN 5 //IR Transmitter pin
#define DHTPIN 4     // Digital pin connected to the DHT sensor

#define DHTTYPE DHT11   // DHT 11
DHT dht(DHTPIN, DHTTYPE);

//Setup Daikin AC
//https://github.com/crankyoldgit/IRremoteESP8266/blob/master/examples/TurnOnDaikinAC/TurnOnDaikinAC.ino
const uint16_t kIrLed = IRTXPIN;  // ESP8266 GPIO pin to use. Recommended: 4 (D2). NOTE: ESP32 doesnt use the same pinout.
IRDaikinESP ac(kIrLed);  // Set the GPIO to be used to sending the message

// Store individual 15-second readings temporarily
std::vector<float> tempBuffer;
std::vector<float> humBuffer;
unsigned long lastCollectionTime = 0;
unsigned long lastAverageTime = 0;
unsigned long collectionInterval = 15000;    // 15 seconds
unsigned long averageInterval = 300000;      // 5 minutes

void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch(event) {
    case SYSTEM_EVENT_AP_STACONNECTED:
      Serial.print("Client connected, MAC: ");
      Serial.println(WiFi.softAPgetStationNum());
      break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
      Serial.print("Client disconnected, MAC: ");
      Serial.println(WiFi.softAPgetStationNum());
      break;
    default:
      break;
  }
}

void setupMulticastDNS() {
  Serial.println("Starting mDNS responder...");
  if (!MDNS.begin("smartac")) { // Start the mDNS responder for smartac.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started for smartac.local");
}



void setup() {
  Serial.begin(115200);
  Serial.println("Starting SmartAC Remote");

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }
  Serial.println("SPIFFS is mounted");

  Serial.println("Loading config...");
  loadConfig();
  Serial.println("Config loaded");
  Serial.println("Loading historical data...");
  load5MinuteData();

  Serial.print("Setting up AP: ");
  if (!WiFi.softAP(local_wifi_ssid.c_str(), local_wifi_password.c_str())) {
    Serial.println("Failed to set up AP");
  }
  WiFi.softAPConfig(ap_local_IP, ap_gateway, ap_subnet);
  WiFi.onEvent(WiFiEvent);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Setup DHT
  dht.begin();
  Serial.println("Started DHT");

  pinMode (LEDPIN, OUTPUT);
  ac.begin();

  //jwt.allocateJWTMemory();
  Serial.println("Starting Web Server...");
  setupWebServer(); //Setup HTTP Routing

  if (config.wifi_ssid.length() > 0) {
    Serial.println("Found saved WiFi credentials for: " + config.wifi_ssid);
    Serial.println("Connecting to WiFi...");
    WiFi.begin(config.wifi_ssid.c_str(), config.wifi_password.c_str());
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi with IP: " + WiFi.localIP().toString());
  }

  setupMulticastDNS();
}

void loop() {

  if ((millis() - lastCollectionTime) > collectionInterval) {
    lastCollectionTime = millis();

    // Read and store data every 15 seconds
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    if (!isnan(temp) && !isnan(hum)) {
      tempBuffer.push_back(temp);
      humBuffer.push_back(hum);
      Serial.printf("Collected data - Temp: %.2f, Humidity: %.2f\n", temp, hum);
    } else {
      Serial.println("Failed to read from DHT sensor.");
    }
  }

  if ((millis() - lastAverageTime) > averageInterval) {
    lastAverageTime = millis();

    // Calculate the average for the last 5 minutes if there is data
    if (!tempBuffer.empty() && !humBuffer.empty()) {
      float tempSum = std::accumulate(tempBuffer.begin(), tempBuffer.end(), 0.0);
      float humSum = std::accumulate(humBuffer.begin(), humBuffer.end(), 0.0);
      float avgTemp = tempSum / tempBuffer.size();
      float avgHum = humSum / humBuffer.size();

      // Store the averaged data in the 5-minute data vectors
      temperatureData.push_back(avgTemp);
      humidityData.push_back(avgHum);
      timestamps.push_back(getCurrentTimestamp());

      Serial.printf("5-Minute Average - Temp: %.2f, Humidity: %.2f\n", avgTemp, avgHum);

      // Save to SPIFFS
      save5MinuteData();

      // Clear buffers for the next 5-minute period
      tempBuffer.clear();
      humBuffer.clear();
    } else {
      Serial.println("Insufficient data for averaging.");
    }
  }

/*
    ac.on();
    ac.setFan(1);
    ac.setMode(kDaikinCool);
    ac.setTemp(25);
    ac.setSwingVertical(false);
    ac.setSwingHorizontal(false);

    Serial.println("Send IR Data...");
    Serial.println(ac.toString());

      // Now send the IR signal.
// #if SEND_DAIKIN
    ac.send();
//#endif  // SEND_DAIKIN

    digitalWrite (LEDPIN, LOW); */

}

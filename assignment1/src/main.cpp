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

unsigned long lastCollectionTime = 0; // Last time data was collected per 15 seconds
unsigned long lastAverageTime = 0;   // Last time data was aggregated to 5min data point
unsigned long lastHourlyAggregation = 0; // Last time data was aggregated to hourly data point
unsigned long last6HourAggregation = 0; // Last time data was aggregated to 6-hour data point
unsigned long lastIRSendTime = 0; // Last time IR signal was sent

unsigned long collectionInterval = 15000;    // 15 seconds
unsigned long averageInterval = 300000;      // 5 minutes
unsigned long irInterval = 10000;            // 10 seconds

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
  loadData();

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

    unsigned long now = millis();

    // Collect data every 15 seconds (5-minute averaging is done here)
    if ((now - lastCollectionTime) > collectionInterval) {
        lastCollectionTime = now;
        
        // Collect and buffer DHT data...
    }

    // Average data every 5 minutes
    if ((now - lastAverageTime) > averageInterval) {
        lastAverageTime = now;

        if (!tempBuffer.empty() && !humBuffer.empty()) {
            // Calculate and store 5-minute average
            float avgTemp = std::accumulate(tempBuffer.begin(), tempBuffer.end(), 0.0) / tempBuffer.size();
            float avgHum = std::accumulate(humBuffer.begin(), humBuffer.end(), 0.0) / humBuffer.size();
            temperatureData5Min.push_back(avgTemp);
            humidityData5Min.push_back(avgHum);
            timestamps5Min.push_back(getCurrentTimestamp());

            Serial.printf("5-Minute Average - Temp: %.2f, Humidity: %.2f\n", avgTemp, avgHum);
            
            // Clear buffers for the next 5-minute period
            tempBuffer.clear();
            humBuffer.clear();
        }
    }

    // Aggregate to hourly data every hour
    if ((now - lastHourlyAggregation) > 3600000) { // 1 hour
        lastHourlyAggregation = now;

        // Aggregate last 12 5-minute points into an hourly average
        if (temperatureData5Min.size() >= 12) {
            float avgTemp = std::accumulate(temperatureData5Min.end() - 12, temperatureData5Min.end(), 0.0) / 12;
            float avgHum = std::accumulate(humidityData5Min.end() - 12, humidityData5Min.end(), 0.0) / 12;
            temperatureDataHourly.push_back(avgTemp);
            humidityDataHourly.push_back(avgHum);
            timestampsHourly.push_back(getCurrentTimestamp());

            Serial.printf("Hourly Average - Temp: %.2f, Humidity: %.2f\n", avgTemp, avgHum);
            
            // Rotate and save 5-minute data only after aggregating it into hourly data
            rotateAndSave5MinuteData();
            rotateAndSaveHourlyData();
        }
    }

    // Aggregate to 6-hour data every 6 hours
    if ((now - last6HourAggregation) > 21600000) { // 6 hours
        last6HourAggregation = now;

        // Aggregate last 6 hourly points into a 6-hour average
        if (temperatureDataHourly.size() >= 6) {
            float avgTemp = std::accumulate(temperatureDataHourly.end() - 6, temperatureDataHourly.end(), 0.0) / 6;
            float avgHum = std::accumulate(humidityDataHourly.end() - 6, humidityDataHourly.end(), 0.0) / 6;
            temperatureData6Hour.push_back(avgTemp);
            humidityData6Hour.push_back(avgHum);
            timestamps6Hour.push_back(getCurrentTimestamp());

            Serial.printf("6-Hour Average - Temp: %.2f, Humidity: %.2f\n", avgTemp, avgHum);
            
            // Rotate and save hourly data only after aggregating it into 6-hour data
            rotateAndSaveHourlyData();
            rotateAndSave6HourData();
        }
    }

    // Send IR signal every 10 seconds
    if ((now - lastIRSendTime) > irInterval) {
        lastIRSendTime = now;

        // Send IR signal to AC
        ac.on();
        ac.setFan(1);
        ac.setMode(kDaikinCool);
        ac.setTemp(25);
        ac.setSwingVertical(false);
        ac.setSwingHorizontal(false);

        //Serial.println("Send IR Data...");
        //Serial.println(ac.toString());

        // Now send the IR signal.
        ac.send();
    }
}

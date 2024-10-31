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

void generateSampleData(const char* path) {
    // Check if file already exists
    if (SPIFFS.exists(path)) {
        Serial.println("Sample data file already exists. Skipping creation.");
        return;
    }

    // Generate sample data points
    std::vector<DataPoint> sampleData;
    uint32_t startTimestamp = 1729696995;
    for (int i = 0; i < 5; i++) { // Create only 5 data points for simplicity
        sampleData.push_back({20.0f + i, 50.0f + i * 5, startTimestamp + i * 300});
    }

    // Save data to SPIFFS
    saveDataPoints(path, sampleData);
}




void setup() {
  Serial.begin(115200);
  Serial.println("Starting SmartAC Remote");

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }
  Serial.println("SPIFFS is mounted");

  const char* path = "/sample_data.bin";
  generateSampleData(path);

  // Load and verify data from SPIFFS
  DataPointHeader header;
  std::vector<DataPoint> loadedData;
  if (!loadDataPoints(path, header, loadedData)) {
      Serial.println("Failed to load sample data");
      return;
  } else {
      Serial.printf("Loaded %d sample data points\n", loadedData.size());
  }

  // Display loaded data and compare to sample
  bool dataMatches = true;
  Serial.println("Verifying loaded data points...");
  for (size_t i = 0; i < loadedData.size(); ++i) {
      float expectedTemp = 20.0f + i;
      float expectedHum = 50.0f + i * 5;
      uint32_t expectedTimestamp = 1729696995 + i * 300;
      
      if (loadedData[i].temperature != expectedTemp || 
          loadedData[i].humidity != expectedHum || 
          loadedData[i].timestamp != expectedTimestamp) {
          Serial.printf("Data mismatch at index %d - Expected Temp: %.2f, Hum: %.2f, Timestamp: %u\n", 
                        i, expectedTemp, expectedHum, expectedTimestamp);
          dataMatches = false;
      }
  }

  if (dataMatches) {
      Serial.println("Test PASSED: Loaded data matches generated sample data.");
  } else {
      Serial.println("Test FAILED: Loaded data does not match generated sample data.");
  }

  
  Serial.println("Loading config...");
  loadConfig();
  Serial.println("Config loaded");
  Serial.println("Loading historical data...");
  loadHistoricalData();

  // Check if we have any historical data and set last aggregation times
  if (!temperatureData5Min.empty()) {
      lastAverageTime = millis() - (millis() - calculateNextInterval(temperatureData5Min.back().timestamp, averageInterval));
      Serial.printf("Setting last 5-minute aggregation: %lu\n", lastAverageTime);
  }
  if (!temperatureDataHourly.empty()) {
      lastHourlyAggregation = millis() - (millis() - calculateNextInterval(temperatureDataHourly.back().timestamp, 3600000)); // 1 hour
      Serial.printf("Setting last hourly aggregation: %lu\n", lastHourlyAggregation);
  }
  if (!temperatureData6Hour.empty()) {
      last6HourAggregation = millis() - (millis() - calculateNextInterval(temperatureData6Hour.back().timestamp, 21600000)); // 6 hours
      Serial.printf("Setting last 6-hour aggregation: %lu\n", last6HourAggregation);
  }

  Serial.println("Historical data loaded. Ready to start data collection.");
  Serial.printf("Loaded %d 5-minute data points\n", temperatureData5Min.size());
  Serial.printf("Loaded %d hourly data points\n", temperatureDataHourly.size());
  Serial.printf("Loaded %d 6-hour data points\n", temperatureData6Hour.size());


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
  setupMulticastDNS();


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

  // Configure time with NTP
  // Timezone for Australia/Sydney: https://github.com/nayarsystems/posix_tz_db/blob/1f0cc11d79f7384afcf6acd860d8565165d940db/zones.csv#L317
  // AEST-10AEDT,M10.1.0,M4.1.0/3
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");  // Adjust time offset if needed (e.g., UTC offset)

  // Wait for time to be set
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      return;
  }
  Serial.println("Time initialized with NTP");

  Serial.println(&timeinfo, "UTC Time is: %A, %B %d %Y %H:%M:%S zone %Z (%z)");
  Serial.println("Setting timezone to " + config.timezone);
  Serial.println("Setting POSIX timezone to " + config.posix_tz);
  //Now we can set timezone
  //https://randomnerdtutorials.com/esp32-ntp-timezones-daylight-saving/
  setenv("TZ", config.posix_tz.c_str(), 1);
  tzset();
  Serial.println(&timeinfo, "Local Time is: %A, %B %d %Y %H:%M:%S zone %Z (%z)");
  Serial.println("SmartAC Remote is ready");
}

void loop() {

    unsigned long now = millis();

    // Collect data every 15 seconds (5-minute averaging is done here)
    if ((now - lastCollectionTime) > collectionInterval) {
        lastCollectionTime = now;
        
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

    // Average data every 5 minutes
    if ((now - lastAverageTime) > averageInterval) {
        lastAverageTime = now;

        if (!tempBuffer.empty() && !humBuffer.empty()) {
            // Calculate and store 5-minute average
            float avgTemp = std::accumulate(tempBuffer.begin(), tempBuffer.end(), 0.0) / tempBuffer.size();
            float avgHum = std::accumulate(humBuffer.begin(), humBuffer.end(), 0.0) / humBuffer.size();
            //DataPoint newPoint = {avgTemp, avgHum, getCurrentEpoch()};
            temperatureData5Min.push_back({avgTemp, avgHum, getCurrentEpoch()});

            Serial.printf("5-Minute Average - Temp: %.2f, Humidity: %.2f\n", avgTemp, avgHum);

            // Save the latest 5-minute data to SPIFFS
            saveDataPoints("/data_5min.bin", temperatureData5Min);
            
            // Clear buffers for the next 5-minute period
            tempBuffer.clear();
            humBuffer.clear();
        }
    }

    // Aggregate to hourly data every hour
    if ((now - lastHourlyAggregation) >= 3600000) { // 1 hour
      lastHourlyAggregation = now;

      // Aggregate the last 12 5-minute points into an hourly average
      if (temperatureData5Min.size() >= 12) {
            float tempSum = 0.0;
            float humSum = 0.0;

            // Sum temperature and humidity from the last 12 5 min DataPoints
            for (auto it = temperatureData5Min.end() - 12; it != temperatureData5Min.end(); ++it) {
                tempSum += it->temperature;
                humSum += it->humidity;
            }

            // Calculate averages and add new 1-hour DataPoint
            float avgTemp = tempSum / 12;
            float avgHum = humSum / 12;
            temperatureDataHourly.push_back({avgTemp, avgHum, getCurrentEpoch()});


          Serial.printf("Hourly Average - Temp: %.2f, Humidity: %.2f\n", avgTemp, avgHum);

          saveDataPoints("/data_hourly.bin", temperatureDataHourly);

          // Rotate old points now that we have aggregated
          while (temperatureData5Min.size() > MAX_5MIN_POINTS) {
              temperatureData5Min.erase(temperatureData5Min.begin());
          }
      }
    }


    // Aggregate to 6-hour data every 6 hours
    if ((now - last6HourAggregation) >= 21600000) { // 6 hours
        last6HourAggregation = now;

        if (temperatureDataHourly.size() >= 6) {  // Ensure we have enough hourly data for 6-hour aggregation
            float tempSum = 0.0;
            float humSum = 0.0;

            // Sum temperature and humidity from the last 6 hourly DataPoints
            for (auto it = temperatureDataHourly.end() - 6; it != temperatureDataHourly.end(); ++it) {
                tempSum += it->temperature;
                humSum += it->humidity;
            }

            // Calculate averages and add new 6-hour DataPoint
            float avgTemp = tempSum / 6;
            float avgHum = humSum / 6;
            temperatureData6Hour.push_back({avgTemp, avgHum, getCurrentEpoch()});

            Serial.printf("6-Hour Average - Temp: %.2f, Humidity: %.2f\n", avgTemp, avgHum);

            saveDataPoints("/data_6hour.bin", temperatureData6Hour);

        }

        //Now we've done our aggregation, rotate old points.
        while (temperatureDataHourly.size() > MAX_HOURLY_POINTS) {
            temperatureDataHourly.erase(temperatureDataHourly.begin());
        }

        while (temperatureData6Hour.size() > MAX_6HOUR_POINTS) {
            temperatureData6Hour.erase(temperatureData6Hour.begin());
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

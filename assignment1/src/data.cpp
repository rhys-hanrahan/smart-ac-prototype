// data.cpp
#include <Arduino.h>
#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <time.h>
#include <data.h>
#include <vector>

std::vector<float> temperatureData = {22.5, 23.0, 22.8, 23.1};
std::vector<float> humidityData = {55.0, 56.2, 54.8, 55.5};
std::vector<String> timestamps = {"10:00", "10:30", "11:00", "11:30"};

// Define and initialize the activity log
std::vector<ActivityLogEntry> activityLog = {
  {"10:05", "Turn AC On", "Temperature: 22.5°C"},
  {"10:35", "Adjust Temp Up", "New Temperature: 23.0°C"},
  {"11:10", "Turn AC Off", "Temperature stable"},
};

void save5MinuteData() {
  StaticJsonDocument<4096> doc;
  JsonArray tempArray = doc.createNestedArray("temperatureData");
  JsonArray humArray = doc.createNestedArray("humidityData");
  JsonArray timeArray = doc.createNestedArray("timestamps");

  // Add data to JSON arrays
  for (float temp : temperatureData) tempArray.add(temp);
  for (float hum : humidityData) humArray.add(hum);
  for (String ts : timestamps) timeArray.add(ts);

  // Serialize JSON document to string
  File file = SPIFFS.open("/data.json", FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open data.json for writing");
    return;
  }
  if (serializeJson(doc, file) == 0) {
    Serial.println("Failed to write data to file");
  }
  file.close();
}

void load5MinuteData() {
  if (!SPIFFS.exists("/data.json")) {
    Serial.println("No data.json file found, starting with empty dataset.");
    return;
  }

  File file = SPIFFS.open("/data.json", FILE_READ);
  if (!file) {
    Serial.println("Failed to open data.json for reading");
    return;
  }

  StaticJsonDocument<4096> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.println("Failed to parse JSON data.");
    return;
  }

  // Load data from JSON arrays
  for (float temp : doc["temperatureData"].as<JsonArray>()) temperatureData.push_back(temp);
  for (float hum : doc["humidityData"].as<JsonArray>()) humidityData.push_back(hum);
  for (String ts : doc["timestamps"].as<JsonArray>()) timestamps.push_back(ts);

  Serial.println("Loaded historical data from SPIFFS.");
}

String getCurrentTimestamp() {
  time_t now = time(nullptr);
  struct tm* timeInfo = localtime(&now);
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", timeInfo);
  return String(buffer);
}

// Helper function to filter data with a dynamic limit
template <typename T>
void filterData(const std::vector<T>& source, std::vector<T>& destination, int maxPoints) {
    int startIndex = std::max(0, (int)source.size() - maxPoints);
    for (int i = startIndex; i < source.size(); ++i) {
        destination.push_back(source[i]);
    }
}

// Filter last 24 hours (288 data points) for "day" data
void filterDataForDay(std::vector<float>& tempData, std::vector<float>& humData, std::vector<String>& timestamps) {
    int dataPoints = 288; // 24 hours * 12 (5-minute intervals in each hour)
    filterData(temperatureData, tempData, dataPoints);
    filterData(humidityData, humData, dataPoints);
    filterData(timestamps, timestamps, dataPoints);
}

// Filter last 7 days (2016 data points) for "week" data
void filterDataForWeek(std::vector<float>& tempData, std::vector<float>& humData, std::vector<String>& timestamps) {
    int dataPoints = 2016; // 7 days * 24 hours * 12 intervals per hour
    filterData(temperatureData, tempData, dataPoints);
    filterData(humidityData, humData, dataPoints);
    filterData(timestamps, timestamps, dataPoints);
}

// Filter last 30 days (8640 data points) for "month" data
void filterDataForMonth(std::vector<float>& tempData, std::vector<float>& humData, std::vector<String>& timestamps) {
    int dataPoints = 8640; // 30 days * 24 hours * 12 intervals per hour
    filterData(temperatureData, tempData, dataPoints);
    filterData(humidityData, humData, dataPoints);
    filterData(timestamps, timestamps, dataPoints);
}

// Filter last 365 days (105120 data points) for "year" data
void filterDataForYear(std::vector<float>& tempData, std::vector<float>& humData, std::vector<String>& timestamps) {
    int dataPoints = 105120; // 365 days * 24 hours * 12 intervals per hour
    filterData(temperatureData, tempData, dataPoints);
    filterData(humidityData, humData, dataPoints);
    filterData(timestamps, timestamps, dataPoints);
}


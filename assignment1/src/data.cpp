// data.cpp
#include <Arduino.h>
#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <time.h>
#include <data.h>
#include <vector>
#include <numeric>

// Define and initialize the activity log
std::vector<ActivityLogEntry> activityLog = {
  {"10:05", "Turn AC On", "Temperature: 22.5°C"},
  {"10:35", "Adjust Temp Up", "New Temperature: 23.0°C"},
  {"11:10", "Turn AC Off", "Temperature stable"},
};

// Maximum points to retain for each data interval
const size_t MAX_5MIN_POINTS = 2016;   // 1 week
const size_t MAX_HOURLY_POINTS = 720;  // 1 month
const size_t MAX_6HOUR_POINTS = 1460;  // 1 year


// Define global data vectors
std::vector<float> temperatureData5Min;
std::vector<float> humidityData5Min;
std::vector<String> timestamps5Min;

std::vector<float> temperatureDataHourly;
std::vector<float> humidityDataHourly;
std::vector<String> timestampsHourly;

std::vector<float> temperatureData6Hour;
std::vector<float> humidityData6Hour;
std::vector<String> timestamps6Hour;

// Helper function to get the current timestamp as a String
String getCurrentTimestamp() {
  time_t now = time(nullptr);
  struct tm* timeInfo = localtime(&now);
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", timeInfo);
  return String(buffer);
}

// Load data from SPIFFS on boot
void loadData() {
    Serial.println("Loading data from SPIFFS");

    // Load 5-minute data
    loadJsonData("/data_5min.json", temperatureData5Min, humidityData5Min, timestamps5Min, 132000, "5-minute");

    // Load hourly data
    loadJsonData("/data_hourly.json", temperatureDataHourly, humidityDataHourly, timestampsHourly, 65535, "hourly");

    // Load 6-hour data
    loadJsonData("/data_6hour.json", temperatureData6Hour, humidityData6Hour, timestamps6Hour, 128000, "6-hour");
}

// Helper function to load JSON data from a file on SPIFFS
void loadJsonData(const char* path, std::vector<float>& temperatureData, std::vector<float>& humidityData,
                  std::vector<String>& timestamps, size_t jsonCapacity, const char* label) {
    File file = SPIFFS.open(path, FILE_READ);
    if (!file) {
        Serial.printf("Failed to open %s\n", path);
        return;
    }

    // Use DynamicJsonDocument to allocate memory on the heap
    DynamicJsonDocument* doc = new DynamicJsonDocument(jsonCapacity);
    if (doc->capacity() == 0) {
        Serial.printf("Failed to allocate memory for %s data\n", label);
        delete doc;  // Clean up to avoid memory leaks
        return;
    }

    DeserializationError error = deserializeJson(*doc, file);
    file.close();

    if (error) {
        Serial.printf("Failed to deserialize %s data: %s\n", label, error.c_str());
        delete doc; //Free up memory
        return;
    }

    // Clear existing data to avoid appending duplicate entries
    temperatureData.clear();
    humidityData.clear();
    timestamps.clear();

    // Load data into vectors
    for (float temp : (*doc)["temperature"].as<JsonArray>()) temperatureData.push_back(temp);
    for (float hum : (*doc)["humidity"].as<JsonArray>()) humidityData.push_back(hum);
    for (String ts : (*doc)["timestamps"].as<JsonArray>()) timestamps.push_back(ts);

    Serial.printf("Loaded %d %s data points\n", temperatureData.size(), label);

    delete doc; // Free up memory
}

void rotateAndSave5MinuteData() {
  // Rotate out old data if we exceed the maximum points
  while (temperatureData5Min.size() > MAX_5MIN_POINTS) {
    temperatureData5Min.erase(temperatureData5Min.begin());
    humidityData5Min.erase(humidityData5Min.begin());
    timestamps5Min.erase(timestamps5Min.begin());
  }

  // Save the latest 5-minute data to SPIFFS
  // Allocate the DynamicJsonDocument on the heap
  DynamicJsonDocument* doc = new DynamicJsonDocument(8192);  // Adjust capacity if needed

  if (doc->capacity() == 0) {
    Serial.println("Failed to allocate memory for 5-minute data");
    delete doc;
    return;
  }

  JsonArray tempArray = doc->createNestedArray("temperature");
  JsonArray humArray = doc->createNestedArray("humidity");
  JsonArray timeArray = doc->createNestedArray("timestamps");

  for (float temp : temperatureData5Min) tempArray.add(temp);
  for (float hum : humidityData5Min) humArray.add(hum);
  for (String ts : timestamps5Min) timeArray.add(ts);

  safeWriteToFile("/data_5min.json", *doc);
  delete doc; // Free up memory
}

void rotateAndSaveHourlyData() {
  // Rotate out old data if we exceed the maximum points
  while (temperatureDataHourly.size() > MAX_HOURLY_POINTS) {
    temperatureDataHourly.erase(temperatureDataHourly.begin());
    humidityDataHourly.erase(humidityDataHourly.begin());
    timestampsHourly.erase(timestampsHourly.begin());
  }

  // Save the latest hourly data to SPIFFS
  // Allocate the DynamicJsonDocument on the heap
  DynamicJsonDocument* doc = new DynamicJsonDocument(4096);  // Adjust capacity if needed

  if (doc->capacity() == 0) {
    Serial.println("Failed to allocate memory for hourly data");
    delete doc;
    return;
  }

  JsonArray tempArray = doc->createNestedArray("temperature");
  JsonArray humArray = doc->createNestedArray("humidity");
  JsonArray timeArray = doc->createNestedArray("timestamps");

  for (float temp : temperatureDataHourly) tempArray.add(temp);
  for (float hum : humidityDataHourly) humArray.add(hum);
  for (String ts : timestampsHourly) timeArray.add(ts);

  safeWriteToFile("/data_hourly.json", *doc);
  delete doc; // Free up memory
}

void rotateAndSave6HourData() {
  // Rotate out old data if we exceed the maximum points
  while (temperatureData6Hour.size() > MAX_6HOUR_POINTS) {
    temperatureData6Hour.erase(temperatureData6Hour.begin());
    humidityData6Hour.erase(humidityData6Hour.begin());
    timestamps6Hour.erase(timestamps6Hour.begin());
  }

  // Save the latest 6-hour data to SPIFFS
  // Allocate the DynamicJsonDocument on the heap
  DynamicJsonDocument* doc = new DynamicJsonDocument(6144);  // Adjust capacity if needed

  if (doc->capacity() == 0) {
    Serial.println("Failed to allocate memory for 6-hour data");
    delete doc;
    return;
  }

  JsonArray tempArray = doc->createNestedArray("temperature");
  JsonArray humArray = doc->createNestedArray("humidity");
  JsonArray timeArray = doc->createNestedArray("timestamps");

  for (float temp : temperatureData6Hour) tempArray.add(temp);
  for (float hum : humidityData6Hour) humArray.add(hum);
  for (String ts : timestamps6Hour) timeArray.add(ts);

  safeWriteToFile("/data_6hour.json", *doc);
  delete doc; // Free up memory
}

// Aggregate 5-minute data into an hourly data point
void aggregateToHourlyData() {
  if (temperatureData5Min.size() >= 12) {
    float avgTemp = std::accumulate(temperatureData5Min.end() - 12, temperatureData5Min.end(), 0.0) / 12;
    float avgHum = std::accumulate(humidityData5Min.end() - 12, humidityData5Min.end(), 0.0) / 12;
    temperatureDataHourly.push_back(avgTemp);
    humidityDataHourly.push_back(avgHum);
    timestampsHourly.push_back(getCurrentTimestamp());

    // Rotate and save hourly data to SPIFFS
    rotateAndSaveHourlyData();
  }
}

// Aggregate hourly data into a 6-hour data point
void aggregateTo6HourData() {
  if (temperatureDataHourly.size() >= 6) {
    float avgTemp = std::accumulate(temperatureDataHourly.end() - 6, temperatureDataHourly.end(), 0.0) / 6;
    float avgHum = std::accumulate(humidityDataHourly.end() - 6, humidityDataHourly.end(), 0.0) / 6;
    temperatureData6Hour.push_back(avgTemp);
    humidityData6Hour.push_back(avgHum);
    timestamps6Hour.push_back(getCurrentTimestamp());

    // Rotate and save 6-hour data to SPIFFS
    rotateAndSave6HourData();
  }
}

void safeWriteToFile(const char* filePath, const JsonDocument& doc) {
  // Generate a unique temporary file name using the current time
  String tempFileName = "/temp_" + String(millis()) + ".json";

  // Open the temporary file for writing
  File tempFile = SPIFFS.open(tempFileName.c_str(), FILE_WRITE);
  if (!tempFile) {
    Serial.println("Failed to open temporary file for writing");
    return;
  }

  // Serialize JSON document to the temporary file
  if (serializeJson(doc, tempFile) == 0) {
    Serial.println("Failed to write data to temporary file");
    tempFile.close();
    SPIFFS.remove(tempFileName);  // Clean up temporary file on failure
    return;
  }
  tempFile.close();

  // Safely rename the temporary file to the final file name
  SPIFFS.remove(filePath);                    // Remove the old file if it exists
  SPIFFS.rename(tempFileName.c_str(), filePath);  // Rename temporary file to final file
  Serial.println("Data safely written to file");
}

// Helper function to filter data with a dynamic limit
template <typename T>
void filterData(const std::vector<T>& source, std::vector<T>& destination, int maxPoints) {
    int startIndex = std::max(0, (int)source.size() - maxPoints);
    for (int i = startIndex; i < source.size(); ++i) {
        destination.push_back(source[i]);
    }
}

/*
// Filter last 24 hours (288 data points) for "day" data
void filterDataForDay(std::vector<float>& tempData, std::vector<float>& humData, std::vector<String>& timestamps) {
    int dataPoints = 288; // 24 hours * 12 (5-minute intervals in each hour)
    filterData(temperatureData, tempData, dataPoints);
    filterData(humidityData, humData, dataPoints);
    filterData(timestamps, timestamps, dataPoints);
} */

void filterDataForDay(std::vector<float>& tempData, std::vector<float>& humData, std::vector<String>& timeData) {
    size_t start = temperatureData5Min.size() > 288 ? temperatureData5Min.size() - 288 : 0; // 288 points = 24 hours * 12 points/hour
    for (size_t i = start; i < temperatureData5Min.size(); ++i) {
        tempData.push_back(temperatureData5Min[i]);
        humData.push_back(humidityData5Min[i]);
        timeData.push_back(timestamps5Min[i]);
    }
}

void filterDataForWeek(std::vector<float>& tempData, std::vector<float>& humData, std::vector<String>& timeData) {
    size_t start = temperatureDataHourly.size() > 168 ? temperatureDataHourly.size() - 168 : 0; // 168 points = 7 days * 24 points/day
    for (size_t i = start; i < temperatureDataHourly.size(); ++i) {
        tempData.push_back(temperatureDataHourly[i]);
        humData.push_back(humidityDataHourly[i]);
        timeData.push_back(timestampsHourly[i]);
    }
}

void filterDataForMonth(std::vector<float>& tempData, std::vector<float>& humData, std::vector<String>& timeData) {
    size_t start = temperatureDataHourly.size() > 720 ? temperatureDataHourly.size() - 720 : 0; // 720 points = 30 days * 24 points/day
    for (size_t i = start; i < temperatureDataHourly.size(); ++i) {
        tempData.push_back(temperatureDataHourly[i]);
        humData.push_back(humidityDataHourly[i]);
        timeData.push_back(timestampsHourly[i]);
    }
}

void filterDataForYear(std::vector<float>& tempData, std::vector<float>& humData, std::vector<String>& timeData) {
    size_t start = temperatureData6Hour.size() > 1460 ? temperatureData6Hour.size() - 1460 : 0; // 1460 points = 365 days * 4 points/day
    for (size_t i = start; i < temperatureData6Hour.size(); ++i) {
        tempData.push_back(temperatureData6Hour[i]);
        humData.push_back(humidityData6Hour[i]);
        timeData.push_back(timestamps6Hour[i]);
    }
}

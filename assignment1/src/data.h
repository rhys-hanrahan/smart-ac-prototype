#ifndef DATA_H
#define DATA_H

#include <vector>
#include <Arduino.h>
#include <ArduinoJson.h>

// Data vectors for storing 5-minute, hourly, and 6-hourly data
extern std::vector<float> temperatureData5Min;
extern std::vector<float> humidityData5Min;
extern std::vector<String> timestamps5Min;

extern std::vector<float> temperatureDataHourly;
extern std::vector<float> humidityDataHourly;
extern std::vector<String> timestampsHourly;

extern std::vector<float> temperatureData6Hour;
extern std::vector<float> humidityData6Hour;
extern std::vector<String> timestamps6Hour;

// Function declarations
void safeWriteToFile(const char* filePath, const JsonDocument& doc);
void loadData();
void loadJsonData(const char* path, std::vector<float>& temperatureData, std::vector<float>& humidityData,
                  std::vector<String>& timestamps, size_t jsonCapacity, const char* label);
void rotateAndSave5MinuteData();
void rotateAndSaveHourlyData();
void rotateAndSave6HourData();
void aggregateToHourlyData();
void aggregateTo6HourData();
String getCurrentTimestamp();

struct ActivityLogEntry {
  String timestamp;
  String action;
  String details;
};

// Declare the activity log vector
extern std::vector<ActivityLogEntry> activityLog;

void filterDataForDay(std::vector<float>& tempData, std::vector<float>& humData, std::vector<String>& timeData);
void filterDataForWeek(std::vector<float>& tempData, std::vector<float>& humData, std::vector<String>& timeData);
void filterDataForMonth(std::vector<float>& tempData, std::vector<float>& humData, std::vector<String>& timeData);
void filterDataForYear(std::vector<float>& tempData, std::vector<float>& humData, std::vector<String>& timeData);

#endif
#ifndef DATA_H
#define DATA_H

#include <vector>
#include <Arduino.h>

extern std::vector<float> temperatureData;     // Declare temperature data
extern std::vector<float> humidityData;        // Declare humidity data
extern std::vector<String> timestamps;         // Declare timestamps

struct ActivityLogEntry {
  String timestamp;
  String action;
  String details;
};

// Declare the activity log vector
extern std::vector<ActivityLogEntry> activityLog;

void filterDataForDay(std::vector<float>& tempData, std::vector<float>& humData, std::vector<String>& timestamps);
void filterDataForWeek(std::vector<float>& tempData, std::vector<float>& humData, std::vector<String>& timestamps);
void filterDataForMonth(std::vector<float>& tempData, std::vector<float>& humData, std::vector<String>& timestamps);
void filterDataForYear(std::vector<float>& tempData, std::vector<float>& humData, std::vector<String>& timestamps);

void safeWriteToFile(const char* filePath, const JsonDocument& doc);
void load5MinuteData();
void save5MinuteData();
String getCurrentTimestamp();

#endif
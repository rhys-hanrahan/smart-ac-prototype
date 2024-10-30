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

#endif
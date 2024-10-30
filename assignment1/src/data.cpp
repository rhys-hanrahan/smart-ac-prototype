// data.cpp
#include <Arduino.h>
#include <vector>
#include "data.h"

std::vector<float> temperatureData = {22.5, 23.0, 22.8, 23.1};
std::vector<float> humidityData = {55.0, 56.2, 54.8, 55.5};
std::vector<String> timestamps = {"10:00", "10:30", "11:00", "11:30"};

// Define and initialize the activity log
std::vector<ActivityLogEntry> activityLog = {
  {"10:05", "Turn AC On", "Temperature: 22.5°C"},
  {"10:35", "Adjust Temp Up", "New Temperature: 23.0°C"},
  {"11:10", "Turn AC Off", "Temperature stable"},
};

#ifndef DATA_H
#define DATA_H

#include <vector>
#include <Arduino.h>
#include <ArduinoJson.h>

//We are using a custom binary format to store data points in SPIFFS. This is needed
//to conserve memory and storage on the SPIFFS filesystem. The format is as follows:
//  - Header: DataPointHeader struct
//  - Data points: DataPoint struct
//This struct defines the header for the data file
struct DataPointHeader {
    uint16_t version;            // File format version
    uint32_t recordCount;        // Number of records in the file
    uint32_t firstTimestamp;     // Unix timestamp of the first data point
    uint32_t lastTimestamp;      // Unix timestamp of the last data point
    uint32_t checksum;           // checksum for data integrity
};

//This struct defines a single data point
struct DataPoint {
    float temperature;           // Temperature in °C
    float humidity;              // Humidity in %
    uint32_t timestamp;          // Unix timestamp for the data point
};

// Data vectors for storing 5-minute, hourly, and 6-hourly data
extern std::vector<DataPoint> temperatureData5Min;
extern std::vector<DataPoint> temperatureDataHourly;
extern std::vector<DataPoint> temperatureData6Hour;

// Maximum points to retain for each data interval
const size_t MAX_5MIN_POINTS = 2016;   // 1 week
const size_t MAX_HOURLY_POINTS = 720;  // 1 month
const size_t MAX_6HOUR_POINTS = 1460;  // 1 year

// Data point load/save
void saveDataPoints(const char* path, const std::vector<DataPoint>& data);
std::vector<DataPoint> loadDataPoints(const char* path, DataPointHeader& header);
void loadHistoricalData();

// Function declarations
void safeWriteToFile(const char* filePath, const JsonDocument& doc);
void loadJsonData(const char* path, std::vector<float>& temperatureData, std::vector<float>& humidityData,
                  std::vector<String>& timestamps, size_t jsonCapacity, const char* label);
void rotateAndSave5MinuteData();
void rotateAndSaveHourlyData();
void rotateAndSave6HourData();
void aggregateToHourlyData();
void aggregateTo6HourData();
String getCurrentTimestamp();
uint32_t getCurrentEpoch();
uint32_t calculateCRC32(const uint8_t* data, size_t length);
unsigned long calculateNextInterval(uint32_t lastTimestamp, unsigned long interval);
String formatTimestamp(uint32_t epoch, const String& timezone = "UTC");

struct ActivityLogEntry {
  String timestamp;
  String action;
  String details;
};

// Declare the activity log vector
extern std::vector<ActivityLogEntry> activityLog;


#endif
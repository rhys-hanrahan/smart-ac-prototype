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

// Define global data vectors
std::vector<DataPoint> temperatureData5Min;
std::vector<DataPoint> temperatureDataHourly;
std::vector<DataPoint> temperatureData6Hour;


void saveDataPoints(const char* path, const std::vector<DataPoint>& data) {
    String tempPath = String(path) + String(millis()) + ".tmp";
    File tempFile = SPIFFS.open(tempPath.c_str(), FILE_WRITE);
    if (!tempFile) {
        Serial.println("Failed to open temporary file for writing");
        return;
    }

    // Calculate the checksum over the data points
    uint32_t checksum = calculateCRC32((uint8_t*)data.data(), data.size() * sizeof(DataPoint));
    Serial.printf("Checksum calculated during save: 0x%08X\n", checksum);

    // Create and write DataPointHeader with checksum
    DataPointHeader header;
    header.version = 1;
    header.recordCount = data.size();
    header.firstTimestamp = data.empty() ? 0 : data.front().timestamp;
    header.lastTimestamp = data.empty() ? 0 : data.back().timestamp;
    header.checksum = checksum;

    tempFile.write((uint8_t*)&header, sizeof(DataPointHeader));

    // Write each data point
    for (const auto& dp : data) {
        tempFile.write((uint8_t*)&dp, sizeof(DataPoint));
    }
    tempFile.close();

    // Rename temporary file to final path (safe write)
    SPIFFS.remove(path);                    // Remove old file if it exists
    SPIFFS.rename(tempPath.c_str(), path);  // Rename temp file to final file name

    Serial.printf("Safely wrote %d data points with checksum 0x%08X to file\n", data.size(), checksum);
}

//Returns 1 on success, 0 on failure
int loadDataPoints(const char* path, DataPointHeader& header, std::vector<DataPoint>& data) {
    data.clear(); // Clear existing data

    File file = SPIFFS.open(path, FILE_READ);
    if (!file) {
        Serial.println("Failed to open file for reading");
        return 0;
    }

    // Read DataPointHeader
    file.read((uint8_t*)&header, sizeof(DataPointHeader));
    Serial.printf("Expected checksum from header: 0x%08X\n", header.checksum);

    // Check the version (optional)
    if (header.version != 1) {
        Serial.println("Unsupported file version");
        file.close();
        return 0;
    }

    // Read each data point
    while (file.available()) {
        DataPoint dp;
        file.read((uint8_t*)&dp, sizeof(DataPoint));
        data.push_back(dp);
    }

    file.close();
    Serial.printf("Read %d data points from file\n", data.size());

    // Print a sample of the loaded data points for verification
    if (!data.empty()) {
        Serial.println("Sample loaded data points:");
        for (size_t i = 0; i < 5 && i < data.size(); ++i) {
            Serial.printf("DataPoint %d - Temp: %.2f, Hum: %.2f, Timestamp: %u\n",
                        i, data[i].temperature, data[i].humidity, data[i].timestamp);
        }
    }

    // Check file size to ensure it matches expected size
    size_t expectedSize = sizeof(DataPointHeader) + (header.recordCount * sizeof(DataPoint));
    // Reopen to check file size immediately after writing
    File checkFile = SPIFFS.open(path, FILE_READ);
    if (checkFile) {
        Serial.printf("File %s read with size: %d bytes\n", path, checkFile.size());
        if (checkFile.size() != expectedSize) {
            Serial.printf("File size mismatch! Expected: %d bytes, Actual: %d bytes\n", expectedSize, checkFile.size());
        } else {
            Serial.println("File size matches expected size");
        }
        checkFile.close();
    } else {
        Serial.println("Failed to reopen file for size check.");
    }

    // Verify checksum
    uint32_t calculatedChecksum = calculateCRC32((uint8_t*)data.data(), data.size() * sizeof(DataPoint));
    if (calculatedChecksum != header.checksum) {
        Serial.printf("Checksum mismatch! Expected 0x%08X, but calculated 0x%08X\n", header.checksum, calculatedChecksum);
        data.clear(); // Clear data as it may be corrupted
    } else {
        Serial.printf("Checksum verified: 0x%08X\n", calculatedChecksum);
    }

    return 1;
}

// Helper function to get the current timestamp as a String
String getCurrentTimestamp() {
  time_t now = time(nullptr);
  struct tm* timeInfo = localtime(&now);
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", timeInfo);
  return String(buffer);
}

uint32_t getCurrentEpoch() {
    // Ensure time is initialized
    time_t now = time(nullptr);
    if (now < 1000000000) { // A basic check if time is initialized (e.g., after 2001-09-09)
        Serial.println("Error: Time not initialized");
        return 0; // Return 0 or a default value if time is not yet set
    }
    return static_cast<uint32_t>(now);
}


// Load data from SPIFFS on boot
void loadHistoricalData() {
    DataPointHeader header;

    // Load 5-minute data
    Serial.println("Loading 5-minute data...");
    if (!loadDataPoints("/data_5min.bin", header, temperatureData5Min)) {
        Serial.println("Failed to load 5-minute data");
    } else {
        Serial.printf("Loaded %d 5-minute data points\n", temperatureData5Min.size());
    }
    // Load hourly data
    Serial.println("Loading hourly data...");
    if (!loadDataPoints("/data_hourly.bin", header, temperatureData5Min)) {
        Serial.println("Failed to load hourly data");
    } else {
        Serial.printf("Loaded %d hourly data points\n", temperatureDataHourly.size());
    }

    // Load 6-hour data
    Serial.println("Loading 6-hour data...");
    if (!loadDataPoints("/data_6hour.bin", header, temperatureData6Hour)) {
        Serial.println("Failed to load 6-hour data");
    } else {
        Serial.printf("Loaded %d 6-hour data points\n", temperatureData6Hour.size());
    }
}

void rotateAndSave5MinuteData() {
    // Rotate out old data if we exceed the maximum points
    while (temperatureData5Min.size() > MAX_5MIN_POINTS) {
        temperatureData5Min.erase(temperatureData5Min.begin());
    }


}

void rotateAndSaveHourlyData() {
    // Rotate out old data if we exceed the maximum points
    while (temperatureDataHourly.size() > MAX_HOURLY_POINTS) {
        temperatureDataHourly.erase(temperatureDataHourly.begin());
    }

    // Save the latest hourly data to SPIFFS
    saveDataPoints("/data_hourly.bin", temperatureDataHourly);
}

void rotateAndSave6HourData() {
    // Rotate out old data if we exceed the maximum points
    while (temperatureData6Hour.size() > MAX_6HOUR_POINTS) {
        temperatureData6Hour.erase(temperatureData6Hour.begin());
    }

    // Save the latest 6-hour data to SPIFFS
    saveDataPoints("/data_6hour.bin", temperatureData6Hour);
}

// Aggregate 5-minute data into an hourly data point
void aggregateToHourlyData() {
    // Check if we have enough 5-minute data points to aggregate into an hourly point
    if (temperatureData5Min.size() >= 12) {
        float tempSum = 0.0;
        float humSum = 0.0;

        // Sum up the last 12 five-minute points to calculate the average
        for (size_t i = temperatureData5Min.size() - 12; i < temperatureData5Min.size(); ++i) {
            tempSum += temperatureData5Min[i].temperature;
            humSum += temperatureData5Min[i].humidity;
        }

        // Calculate averages
        float avgTemp = tempSum / 12;
        float avgHum = humSum / 12;
        uint32_t timestamp = getCurrentEpoch();

        // Add the new hourly DataPoint to the vector
        temperatureDataHourly.push_back({avgTemp, avgHum, timestamp});

        // Rotate and save hourly data to SPIFFS
        rotateAndSaveHourlyData();
    }
}

// Aggregate hourly data into a 6-hour data point
void aggregateTo6HourData() {
    // Check if we have enough hourly data points to aggregate into a 6-hour point
    if (temperatureDataHourly.size() >= 6) {
        float tempSum = 0.0;
        float humSum = 0.0;

        // Sum up the last 6 hourly points to calculate the average
        for (size_t i = temperatureDataHourly.size() - 6; i < temperatureDataHourly.size(); ++i) {
            tempSum += temperatureDataHourly[i].temperature;
            humSum += temperatureDataHourly[i].humidity;
        }

        // Calculate averages
        float avgTemp = tempSum / 6;
        float avgHum = humSum / 6;
        uint32_t timestamp = getCurrentEpoch();

        // Add the new 6-hour DataPoint to the vector
        temperatureData6Hour.push_back({avgTemp, avgHum, timestamp});

        // Rotate and save 6-hour data to SPIFFS
        rotateAndSave6HourData();
    }
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

// Helper function to calculate CRC32 checksum
uint32_t calculateCRC32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        uint8_t byte = data[i];
        crc ^= byte;
        for (uint8_t j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

unsigned long calculateNextInterval(uint32_t lastTimestamp, unsigned long interval) {
    unsigned long timeSinceLast = millis() - lastTimestamp;
    if (timeSinceLast < interval) {
        return interval - timeSinceLast;
    }
    return 0; // Ready for aggregation immediately if timeSinceLast >= interval
}

String formatTimestamp(uint32_t epoch, const String& timezone) {
    // Convert the epoch to time structure
    time_t rawTime = epoch;
    struct tm *timeInfo = localtime(&rawTime);

    // Format the time as a string
    char buffer[25];  // Enough space for "YYYY-MM-DD HH:MM:SS ±HH:MM"
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeInfo);

    // Append timezone
    String formattedTime(buffer);
    formattedTime += " " + timezone;

    return formattedTime;
}


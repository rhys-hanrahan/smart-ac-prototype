#include "config.h"
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "data.h"

// Define the config struct with initial values
Config config = {
  .wifi_ssid = "your_ssid",
  .wifi_password = "your_password",
  .web_username = "admin",
  .web_userpass = "admin123",
  .tempSchedule = { {0.0} }  // Initialize all to 0.0 for simplicity
};

// Define local WiFi SSID and password
String local_wifi_ssid = "SmartAC Remote";
String local_wifi_password = "smart123";

// Define Access Point IP address, Gateway, and Subnet mask
IPAddress ap_local_IP(172, 23, 23, 1);
IPAddress ap_gateway(172, 23, 23, 1);
IPAddress ap_subnet(255, 255, 255, 0);

void loadConfig() {
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }
  Serial.println("SPIFFS is mounted");
  
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return;
  }

  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, buf.get());
  if (error) {
    Serial.println("Failed to parse config file");
    return;
  }

  config.web_username = doc["login"]["user"].as<String>();
  config.web_userpass = doc["login"]["password"].as<String>();
  config.wifi_ssid = doc["wifi"]["ssid"].as<String>();
  config.wifi_password = doc["wifi"]["password"].as<String>();
  
  JsonArray schedule = doc["tempSchedule"].as<JsonArray>();
  for (int day = 0; day < 7; day++) {
    JsonArray daySchedule = schedule[day].as<JsonArray>();
    for (int hour = 0; hour < 24; hour++) {
      config.tempSchedule[day][hour] = daySchedule[hour].as<float>();
    }
  }

  configFile.close();
}

DynamicJsonDocument getConfigJson() {
  DynamicJsonDocument doc(1024);
  doc["login"]["user"] = config.web_username;
  doc["login"]["password"] = config.web_userpass;
  doc["wifi"]["ssid"] = config.wifi_ssid;
  doc["wifi"]["password"] = config.wifi_password;

  JsonArray schedule = doc.createNestedArray("tempSchedule");
  for (int day = 0; day < 7; day++) {
    JsonArray daySchedule = schedule.createNestedArray();
    for (int hour = 0; hour < 24; hour++) {
      daySchedule.add(config.tempSchedule[day][hour]);
    }
  }
  return doc;
}

// Save configuration to JSON file in SPIFFS
void saveConfig() {

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return;
  }

  DynamicJsonDocument doc = getConfigJson();
  serializeJson(doc, configFile);
  configFile.close();
}

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>

void loadConfig();
void saveConfig();
void updateConfig(const String& json);
void applyJsonToConfig(const DynamicJsonDocument& doc);
DynamicJsonDocument getConfigJson();

struct Config {
  String wifi_ssid;
  String wifi_password; //Wifi Password
  String web_username; //Web username
  String web_userpass; //Web password
  float tempSchedule[7][24]; // Store temperature settings by day and hour
};

// Extern declarations
extern Config config;               // Declare the config struct
extern String local_wifi_ssid;      // Declare local WiFi SSID
extern String local_wifi_password;  // Declare local WiFi password
extern IPAddress ap_local_IP;       // Declare Access Point IP address
extern IPAddress ap_gateway;        // Declare Gateway IP address
extern IPAddress ap_subnet;         // Declare Subnet mask



#endif
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>

void loadConfig();
void saveConfig();
void updateConfig(const String& json);
void applyJsonToConfig(const DynamicJsonDocument& doc);
String getPosixTzFromTimezone(String timezone);

DynamicJsonDocument getConfigJson();

struct Config {
  String wifi_ssid;
  String wifi_password; //Wifi Password
  String web_username; //Web username
  String web_userpass; //Web password
  String jwt_secret; //JWT Secret
  String timezone; //Timezone
  String posix_tz; //Timezone as per https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
  bool is_dst; //Daylight Saving Time
  int utc_offset; //UTC Offset
};

// Extern declarations
extern Config config;               // Declare the config struct
extern String local_wifi_ssid;      // Declare local WiFi SSID
extern String local_wifi_password;  // Declare local WiFi password
extern IPAddress ap_local_IP;       // Declare Access Point IP address
extern IPAddress ap_gateway;        // Declare Gateway IP address
extern IPAddress ap_subnet;         // Declare Subnet mask



#endif
#include <WiFi.h>
#include "ThingSpeak.h"
#include <DHT.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Daikin.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <ArduinoJson.h>

struct Config {
  String ssid[32];
  String password[32];
  String username;
  String userpass;
  float tempSchedule[7][24]; // Store temperature settings by day and hour
} config;


#define LEDPIN 2
#define IRTXPIN 5 //IR Transmitter pin
#define DHTPIN 4     // Digital pin connected to the DHT sensor

#define DHTTYPE DHT11   // DHT 11
DHT dht(DHTPIN, DHTTYPE);

//Store temp reading
float temp;
float hum;

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 2000;

//Setup Daikin AC
//https://github.com/crankyoldgit/IRremoteESP8266/blob/master/examples/TurnOnDaikinAC/TurnOnDaikinAC.ino
const uint16_t kIrLed = 5;  // ESP8266 GPIO pin to use. Recommended: 4 (D2). NOTE: ESP32 doesnt use the same pinout.
IRDaikinESP ac(kIrLed);  // Set the GPIO to be used to sending the message

//Webserver
AsyncWebServer server(80); // Web server

struct Config {
  char ssid[32];
  char wifi_password[32]; //Wifi Password
  String username; //Web username
  String userpass; //Web password
  float tempSchedule[7][24]; // Store temperature settings by day and hour
} config;


void loadConfig() {
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }
  
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

  config.username = doc["login"]["user"].as<String>();
  config.userpass = doc["login"]["password"].as<String>();
  config.ssid = doc["wifi"]["ssid"].as<String>();
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

// Save configuration to JSON file in SPIFFS
void saveConfig() {
  DynamicJsonDocument doc(1024);
  doc["login"]["user"] = config.username;
  doc["login"]["password"] = config.userpass;
  doc["wifi"]["ssid"] = config.ssid;
  doc["wifi"]["password"] = config.wifi_password;

  JsonArray schedule = doc.createNestedArray("tempSchedule");
  for (int day = 0; day < 7; day++) {
    JsonArray daySchedule = schedule.createNestedArray();
    for (int hour = 0; hour < 24; hour++) {
      daySchedule.add(config.tempSchedule[day][hour]);
    }
  }

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return;
  }

  serializeJson(doc, configFile);
  configFile.close();
}

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Welcome to Smart AC Control!");
  });
  
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Send configuration data to the client
    DynamicJsonDocument doc(1024);
    doc["ssid"] = config.ssid;
    doc["user"] = config.username;
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });
  
  // More endpoints for setting temperature schedules or changing config
  server.begin();
}

void setup() {
  Serial.begin(115200);
  loadConfig();
  // Setup DHP
  dht.begin();
  Serial.print("Started DHT");

  pinMode (LEDPIN, OUTPUT);
  ac.begin();

  setupWebServer(); //Setup HTTP Routing

  Serial.print("Connecting to SSID: ");
  Serial.println(config.ssid);
  WiFi.begin(config.ssid, config.password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
}

void loop() {
  if ((millis() - lastTime) > timerDelay) {
    // Read temperature as Celsius (the default)
    digitalWrite (LEDPIN, HIGH);
    temp = dht.readTemperature();
    Serial.print("Temperature Celsius: ");
    Serial.print(temp);
    Serial.print(" ºC");

    // Read humidity
    hum = dht.readHumidity();

    Serial.print(" - Humidity: ");
    Serial.print(hum);
    Serial.println(" %");
    
    lastTime = millis();

    ac.on();
    ac.setFan(1);
    ac.setMode(kDaikinCool);
    ac.setTemp(25);
    ac.setSwingVertical(false);
    ac.setSwingHorizontal(false);

    Serial.println("Send IR Data...");
    Serial.println(ac.toString());

      // Now send the IR signal.
// #if SEND_DAIKIN
    ac.send();
//#endif  // SEND_DAIKIN

    digitalWrite (LEDPIN, LOW);
  }
}

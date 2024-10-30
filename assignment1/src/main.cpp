#include <WiFi.h>
#include "ThingSpeak.h"
#include <DHT.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Daikin.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <AsyncTCP.h> // https://randomnerdtutorials.com/esp32-esp8266-web-server-http-authentication/
#include <base64.h>
#include <CustomJWT.h>

struct Config {
  String wifi_ssid;
  String wifi_password; //Wifi Password
  String web_username; //Web username
  String web_userpass; //Web password
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

//Webserver state
const char* PARAM_INPUT_1 = "state";

//Setup Daikin AC
//https://github.com/crankyoldgit/IRremoteESP8266/blob/master/examples/TurnOnDaikinAC/TurnOnDaikinAC.ino
const uint16_t kIrLed = IRTXPIN;  // ESP8266 GPIO pin to use. Recommended: 4 (D2). NOTE: ESP32 doesnt use the same pinout.
IRDaikinESP ac(kIrLed);  // Set the GPIO to be used to sending the message

//Local WIFI Server
String ssid = "SmartAC Remote";
String password = "smart123";
IPAddress ap_local_IP(172, 23, 23, 1);      // Set your ESP32 AP IP
IPAddress ap_gateway(172, 23, 23, 1);
IPAddress ap_subnet(255, 255, 255, 0);



//Webserver
AsyncWebServer server(80); // Web server
char jwtSecret[] = "(M279FET1oJYy4r1|5U1O'hg)bof)I1.Fv3:#]Q>7FzZ_9(ba/2G5OC'H?(Q"; // Secret key for signing JWT
CustomJWT jwt(jwtSecret, 256);

String createJWTToken(const String& username) {
  // Define the payload with a JSON structure
  String payload = "{\"username\":\"" + username + "\"}";

  // Allocate memory for JWT generation
  jwt.allocateJWTMemory();

  // Encode the JWT with the payload
  if (jwt.encodeJWT((char*)payload.c_str())) {
    // Return the final output JWT as a String
    String token = jwt.out;
    jwt.clear(); // Clear the memory after encoding
    return token;
  } else {
    Serial.println("Error encoding JWT");
    jwt.clear(); // Clear memory if encoding failed
    return "";
  }
}

// Function to validate the JWT token
bool isValidJWTToken(const String& token) {
  // Allocate memory for JWT decoding
  jwt.allocateJWTMemory();

  // Decode and verify the JWT
  int result = jwt.decodeJWT((char*)token.c_str());

  jwt.clear(); // Clear the memory after decoding

  // Check result; 0 means valid, non-zero indicates an error
  return (result == 0);
}


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

void setupWebServer() {

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("/index.html");
  server.serveStatic("/dashboard", SPIFFS, "/dashboard.html");
  server.serveStatic("/css", SPIFFS, "/css");
  server.serveStatic("/js", SPIFFS, "/js");

  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Send configuration data to the client
    DynamicJsonDocument doc = getConfigJson();
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });

  server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("username", true) && request->hasParam("password", true)) {
      String username = request->getParam("username", true)->value();
      String password = request->getParam("password", true)->value();

      if (username == config.web_username && password == config.web_userpass) {
        // Generate JWT if credentials are valid
        String token = createJWTToken(username);
        if (!token.isEmpty()) {
          String response = "{\"token\":\"" + token + "\"}";
          request->send(200, "application/json", response);
        } else {
          request->send(500, "application/json", "{\"error\":\"Failed to generate token\"}");
        }
      } else {
        request->send(401, "application/json", "{\"error\":\"Invalid credentials\"}");
      }
    } else {
      request->send(400, "application/json", "{\"error\":\"Username and password required\"}");
    }
  });

  // Protected route that requires a valid JWT token
  server.on("/protected", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasHeader("Authorization")) {
      String authHeader = request->header("Authorization");
      if (authHeader.startsWith("Bearer ")) {
        String token = authHeader.substring(7);
        if (isValidJWTToken(token)) {
          request->send(200, "application/json", "{\"message\":\"Access granted to protected route!\"}");
          return;
        }
      }
    }
    request->send(401, "application/json", "{\"error\":\"Invalid or missing token\"}");
  });

  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(404, "text/plain", "404: Not Found");
  });
  
  // More endpoints for setting temperature schedules or changing config
  server.begin();
}

void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch(event) {
    case SYSTEM_EVENT_AP_STACONNECTED:
      Serial.print("Client connected, MAC: ");
      Serial.println(WiFi.softAPgetStationNum());
      break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
      Serial.print("Client disconnected, MAC: ");
      Serial.println(WiFi.softAPgetStationNum());
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting SmartAC Remote");
  Serial.print("Setting up AP: ");
  if (!WiFi.softAP(ssid.c_str(), password.c_str())) {
    Serial.println("Failed to set up AP");
  }
  WiFi.softAPConfig(ap_local_IP, ap_gateway, ap_subnet);
  WiFi.onEvent(WiFiEvent);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  Serial.println("Loading config...");
  loadConfig();
  // Setup DHH
  dht.begin();
  Serial.print("Started DHT");

  pinMode (LEDPIN, OUTPUT);
  ac.begin();

  //jwt.allocateJWTMemory();
  Serial.println("Starting Web Server...");
  setupWebServer(); //Setup HTTP Routing

  if (config.wifi_ssid.length() > 0) {
    Serial.println("Connecting to WiFi...");
    WiFi.begin(config.wifi_ssid.c_str(), config.wifi_password.c_str());
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
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

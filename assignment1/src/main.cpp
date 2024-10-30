#include <Arduino.h>
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
#include <ESPmDNS.h>
#include <vector>
#include <web.h>
#include <data.h>
#include <config.h>

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

void setupMulticastDNS() {
  Serial.println("Starting mDNS responder...");
  if (!MDNS.begin("smartac")) { // Start the mDNS responder for smartac.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started for smartac.local");
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting SmartAC Remote");
  Serial.print("Setting up AP: ");
  if (!WiFi.softAP(local_wifi_ssid.c_str(), local_wifi_password.c_str())) {
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
  Serial.println("Started DHT");

  pinMode (LEDPIN, OUTPUT);
  ac.begin();

  //jwt.allocateJWTMemory();
  Serial.println("Starting Web Server...");
  setupWebServer(); //Setup HTTP Routing

  if (config.wifi_ssid.length() > 0) {
    Serial.println("Found saved WiFi credentials for: " + config.wifi_ssid);
    Serial.println("Connecting to WiFi...");
    WiFi.begin(config.wifi_ssid.c_str(), config.wifi_password.c_str());
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi with IP: " + WiFi.localIP().toString());
  }

  setupMulticastDNS();
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

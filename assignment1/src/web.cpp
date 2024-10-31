#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <AsyncTCP.h> // https://randomnerdtutorials.com/esp32-esp8266-web-server-http-authentication/
#include <base64.h>
#include <CustomJWT.h>
#include <data.h>
#include <config.h>
#include <web.h>

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

// Load HTML file from SPIFFS
String loadHtmlFile(const char* path) {
  File file = SPIFFS.open(path, "r");
  if (!file) {
    Serial.printf("Failed to open file %s\n", path);
    return String();
  }
  String content = file.readString();
  file.close();
  return content;
}

// Serve page by loading template, navbar, and content
void servePage(AsyncWebServerRequest *request, const char* pageTitle, const char* contentPath) {
  // Load the template, navbar, and content
  String html = loadHtmlFile("/template.html");
  String navbar = loadHtmlFile("/navbar.html");
  String pageContent = loadHtmlFile(contentPath);

  // Replace placeholders
  html.replace("{{NAVBAR}}", navbar);
  html.replace("{{PAGE_TITLE}}", pageTitle);
  html.replace("{{PAGE_CONTENT}}", pageContent);

  // Send the final HTML
  request->send(200, "text/html", html);
}

void setupWebServer() {

  // Redirect root ("/") to "/dashboard"
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasHeader("Authorization")) {
      String authHeader = request->header("Authorization");
      if (authHeader.startsWith("Bearer ")) {
        String token = authHeader.substring(7);
        if (isValidJWTToken(token)) {
          request->redirect("/dashboard");
          return;
        }
      }
    }
    //If no token, redirect to login
    request->redirect("/login");
  });

  server.on("/dashboard", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[HTTP] GET /dashboard");

    // If token is valid, serve the dashboard page
    servePage(request, "Dashboard Overview", "/dashboard_content.html");
  });

  server.on("/dashboard.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/dashboard");
  });

  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    servePage(request, "Settings", "/settings_content.html");
  });

  server.on("/rules", HTTP_GET, [](AsyncWebServerRequest *request) {
    servePage(request, "Rules", "/rules_content.html");
  });

  server.serveStatic("/login", SPIFFS, "/login.html");
  server.on("/login.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/login");
  });

  server.serveStatic("/css", SPIFFS, "/css");
  server.serveStatic("/js", SPIFFS, "/js");

  server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("username", true) && request->hasParam("password", true)) {
      String username = request->getParam("username", true)->value();
      String password = request->getParam("password", true)->value();

      if (username == config.web_username && password == config.web_userpass) {
        // Generate JWT if credentials are valid
        String token = createJWTToken(username);
        if (!token.isEmpty()) {
          Serial.println("[HTTP] POST /login - Login successful");
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

server.on("/api/auth-check", HTTP_GET, [](AsyncWebServerRequest *request) {
  if (!request->hasHeader("Authorization")) {
    request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
    return;
  }
  String authHeader = request->header("Authorization");
  if (!authHeader.startsWith("Bearer ") || !isValidJWTToken(authHeader.substring(7))) {
    request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
    return;
  }
  request->send(200, "application/json", "{\"message\":\"Authorized\"}");
});

  // API endpoint to serve temperature, humidity, timestamps, and activity log data
  server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("[HTTP] GET /api/data");

    // Check if the Authorization header is present
    if (!request->hasHeader("Authorization")) {
        Serial.println("[HTTP] GET /api/data - No Authorization header");
        request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }

    // Get the Authorization header and verify the token format
    String authHeader = request->header("Authorization");
    if (!authHeader.startsWith("Bearer ")) {
        Serial.println("[HTTP] GET /api/data - Invalid token format");
        request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }

    // Extract and validate the token
    String token = authHeader.substring(7);
    if (!isValidJWTToken(token)) {
        Serial.println("[HTTP] GET /api/data - Invalid token");
        request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }

    // Retrieve the "period" query parameter to determine the requested time range
    String period = "day"; // Default to "day" if no period is specified
    if (request->hasParam("period")) {
        period = request->getParam("period")->value();
    }
    Serial.printf("[HTTP] GET /api/data - Requested period: %s\n", period.c_str());

    // Select appropriate data vector based on the period
    std::vector<DataPoint>* selectedData;
    if (period == "day") {
        selectedData = &temperatureData5Min; // Use 5-minute data for a day's worth of points
    } else if (period == "week") {
        selectedData = &temperatureDataHourly; // Use hourly data for a week
    } else if (period == "month") {
        selectedData = &temperatureDataHourly; // Use hourly data for a month
    } else if (period == "year") {
        selectedData = &temperatureData6Hour; // Use 6-hour data for a year
    } else {
        request->send(400, "application/json", "{\"error\":\"Invalid period parameter\"}");
        return;
    }
    Serial.printf("[HTTP] GET /api/data - Sending %d data points for period %s\n", selectedData->size(), period.c_str());
    // Set up the response stream
    //NOTE: We are streaming the response to avoid memory issues with large data sets
    AsyncResponseStream *response = request->beginResponseStream("application/json");

    // Start JSON object
    response->print("{\"temperature\":[");
    for (size_t i = 0; i < selectedData->size(); ++i) {
        response->print(selectedData->at(i).temperature);
        if (i < selectedData->size() - 1) {
            response->print(","); // Add comma between items
        }
    }
    response->print("],\"humidity\":[");

    for (size_t i = 0; i < selectedData->size(); ++i) {
        response->print(selectedData->at(i).humidity);
        if (i < selectedData->size() - 1) {
            response->print(",");
        }
    }
    response->print("],\"timestamps\":[");

    for (size_t i = 0; i < selectedData->size(); ++i) {
        response->print("\"" + formatTimestamp(selectedData->at(i).timestamp, "+00:00") + "\"");  // UTC offset as an example
        if (i < selectedData->size() - 1) {
            response->print(",");
        }
    }
    response->print("],\"message\":\"Thanks for using SmartAC Remote!\"}");

    // Send the response
    request->send(response);

  });

    server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("[HTTP] GET /download");
        if (!request->hasParam("file")) {
            request->send(400, "text/plain", "File parameter missing");
            return;
        }

        // Check if the Authorization header is present
        if (!request->hasHeader("Authorization")) {
            Serial.println("[HTTP] GET /download - No Authorization header");
            request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
            return;
        }

        // Get the Authorization header and verify the token format
        String authHeader = request->header("Authorization");
        if (!authHeader.startsWith("Bearer ")) {
            Serial.println("[HTTP] GET /download - Invalid token format");
            request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
            return;
        }

        // Extract and validate the token
        String token = authHeader.substring(7);
        if (!isValidJWTToken(token)) {
            Serial.println("[HTTP] GET /download - Invalid token");
            request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
            return;
        }


        String filename = request->getParam("file")->value();
        String filePath = "/" + filename;
        Serial.printf("[HTTP] GET /download - Requested file: %s\n", filePath.c_str());

        // Check if file exists
        if (!SPIFFS.exists(filePath)) {
            Serial.printf("[HTTP] GET /download - File not found: %s\n", filePath.c_str());
            request->send(404, "text/plain", "File not found");
            return;
        }

        // Send the file to the client
        request->send(SPIFFS, filePath, "application/octet-stream", true);
    });

    // GET request to retrieve the configuration
    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("[HTTP] GET /api/config");

        // Check if the Authorization header is present
        if (!request->hasHeader("Authorization")) {
            Serial.println("[HTTP] GET /api/config - No Authorization header");
            request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
            return;
        }

        // Get the Authorization header and verify the token format
        String authHeader = request->header("Authorization");
        if (!authHeader.startsWith("Bearer ")) {
            Serial.println("[HTTP] GET /api/config - Invalid token format");
            request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
            return;
        }

        // Extract and validate the token
        String token = authHeader.substring(7);
        if (!isValidJWTToken(token)) {
            Serial.println("[HTTP] GET /api/config - Invalid token");
            request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
            return;
        }

        // Retrieve the current configuration as JSON
        DynamicJsonDocument configJson = getConfigJson();

        // Serialize and send the JSON response
        String response;
        serializeJson(configJson, response);
        request->send(200, "application/json", response);
        Serial.println("[HTTP] GET /api/config - Configuration sent");
    });


    // POST request to save the configuration
    server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!request->hasHeader("Authorization")) {
            request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
            return;
        }

        String authHeader = request->header("Authorization");
        String token = authHeader.startsWith("Bearer ") ? authHeader.substring(7) : "";
        if (!isValidJWTToken(token)) {
            request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
            return;
        }

        // Handle the new configuration JSON from the client
        if (request->hasParam("config", true)) {
            String newConfig = request->getParam("config", true)->value();
            updateConfig(newConfig); // Update in-memory and save to SPIFFS
            request->send(200, "application/json", "{\"message\":\"Config updated successfully\"}");
        } else {
            request->send(400, "application/json", "{\"error\":\"Missing config parameter\"}");
        }
    });


  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(404, "text/plain", "404: Not Found");
  });
  
  // More endpoints for setting temperature schedules or changing config
  server.begin();
}
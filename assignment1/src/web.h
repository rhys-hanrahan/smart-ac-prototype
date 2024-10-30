// web.h
#ifndef WEB_H
#define WEB_H

#include <ESPAsyncWebServer.h>
#include <Arduino.h>

void setupWebServer();
String createJWTToken(const String& username);
bool isValidJWTToken(const String& token);
String loadHtmlFile(const char* path);
void servePage(AsyncWebServerRequest *request, const char* pageTitle, const char* contentPath);


#endif


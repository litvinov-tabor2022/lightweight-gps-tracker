#include "OtaUpdater.h"
#include "Constants.h"
#include <WiFi.h>
#include <AsyncElegantOTA.h>

void OtaUpdater::init() {
    WiFi.mode(WIFI_MODE_AP);
    WiFi.softAP(SSID.c_str(), PASSWORD.c_str());
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/json", R"({"status": "OK"})");
    });
}

void OtaUpdater::begin() {
    AsyncElegantOTA.begin(&server);
    server.begin();
}

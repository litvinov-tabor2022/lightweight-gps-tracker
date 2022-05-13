#include "OtaUpdater.h"
#include "Constants.h"
#include <WiFi.h>
#include <AsyncElegantOTA.h>
#include "Tasker.h"

void OtaUpdater::init(StateManager *stManager, Logging::Logger *logger) {
    this->stateManager = stManager;
    WiFi.mode(WIFI_MODE_AP);
    WiFi.softAP(SSID.c_str(), PASSWORD.c_str());
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/json", R"({"status": "OK"})");
    });
}

void OtaUpdater::begin() {
    AsyncElegantOTA.begin(&server);
    server.begin();
    loop();
}

void OtaUpdater::loop() {
    int counter = 0;
    DefaultTasker.loopEvery("device-checker", 500, [&] {
        counter++;
        if(counter < 180) {
            uint8_t noConnectedDevices = WiFi.softAPgetStationNum();
            stateManager->setNumberOfConnectedDevices(noConnectedDevices);
        } else {
            WiFi.mode(WIFI_OFF);
        }
    });
}

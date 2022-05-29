#include <Arduino.h>
#include <Tracker.h>
#include <Preferences.h>

using namespace GPS_TRACKER;

Preferences preferences;
OtaUpdater *otaUpdater;
Tracker tracker;

void setup() {
    Serial.begin(115200);
    preferences.begin("tracker", false);
    int resets = preferences.getInt("rc", 0);
    preferences.putInt("rc", ++resets);
    Serial.printf("Reset counter: %d\n", resets);
    if (resets >= 3) {
        Serial.println("Entering service mode");
        preferences.putInt("rc", 0);
        otaUpdater = new OtaUpdater();
        otaUpdater->init();
        otaUpdater->begin();
    } else {
        delay(500);
        btStop();
        WiFi.disconnect(true);
        WiFi.softAPdisconnect(true);
        preferences.putInt("rc", 0);
        tracker.init();
        if (!tracker.begin()) {
            esp_restart();
        }
    }
}

void loop() {
    tracker.trackerLoop();
}
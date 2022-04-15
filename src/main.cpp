#include <Arduino.h>
#include <Tracker.h>

using namespace GPS_TRACKER;

Tracker tracker;

void setup() {
    Serial.begin(115200);
    tracker.init();
    tracker.begin();
}

void loop() {
}
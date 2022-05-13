#ifndef LIGHTWEIGHT_GPS_TRACKER_OTAUPDATER_H
#define LIGHTWEIGHT_GPS_TRACKER_OTAUPDATER_H

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

class OtaUpdater {
public:
    explicit OtaUpdater() {};

    void init();

    void begin();

private:
    AsyncWebServer server = AsyncWebServer(80);
};


#endif //LIGHTWEIGHT_GPS_TRACKER_OTAUPDATER_H

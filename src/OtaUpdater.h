#ifndef LIGHTWEIGHT_GPS_TRACKER_OTAUPDATER_H
#define LIGHTWEIGHT_GPS_TRACKER_OTAUPDATER_H

#include "StateManager.h"
#include "logger/Logger.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

using namespace GPS_TRACKER;

class OtaUpdater {
public:
    explicit OtaUpdater() {};

    void init(StateManager *stManager, Logging::Logger *logger);

    void begin();

private:
    void loop();

    AsyncWebServer server = AsyncWebServer(80);
    StateManager *stateManager;
};


#endif //LIGHTWEIGHT_GPS_TRACKER_OTAUPDATER_H

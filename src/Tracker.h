#ifndef LIGHTWEIGHT_GPS_TRACKER_TRACKER_H
#define LIGHTWEIGHT_GPS_TRACKER_TRACKER_H

#include "Configuration.h"
#include "WiFi.h"
#include "sim/SIM7000G.h"
#include "SPIFFS.h"
#include "audio/Player.h"
#include "AudioFileSourceSPIFFS.h"
//#include "networking/WebServer.h"

namespace GPS_TRACKER {
    class Tracker {
    public:
        bool init();

        bool begin();

    private:

        void initLogger();

        void initStateManager();

        void initAudio();

        void initModem();

        bool initSPIFFS();

        void initConfiguration();

        void initPins();

        void trackerLoop();

        void registerOnReachedWaypoint();

        String trackerSSID = "TRACKER-N/A";

        bool shouldSleep = false;
        File loggerFile;
        Logging::Logger *logger;
        GPS_TRACKER::ISIM *sim;
        GPS_TRACKER::Configuration *configuration;
        GPS_TRACKER::StateManager *stateManager;
        AudioPlayer::Player *audioPlayer;
        AudioOutputI2S *audioOutput;
        AudioGeneratorMP3 *mp3;
        AudioFileSourceSPIFFS *source;
//    WebServer *webServer;
    };
}

#endif //LIGHTWEIGHT_GPS_TRACKER_TRACKER_H

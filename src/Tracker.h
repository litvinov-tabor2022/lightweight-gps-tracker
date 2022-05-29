#ifndef LIGHTWEIGHT_GPS_TRACKER_TRACKER_H
#define LIGHTWEIGHT_GPS_TRACKER_TRACKER_H

#include "OtaUpdater.h"
#include "Configuration.h"
#include "networking/SIM7000G.h"
#include "SPIFFS.h"
#include "audio/Player.h"
#include "AudioFileSourceSPIFFS.h"

namespace GPS_TRACKER {
    class Tracker {
    public:
        bool init();

        bool begin();

        void trackerLoop();

    private:

        void initLogger();

        void initStateManager();

        void initAudio();

        bool initModem();

        bool initSPIFFS();

        bool initConfiguration();

        void initPins();

        void registerOnReachedWaypoint();

        void restartTracker();

        void sleepTracker();

        String trackerSSID = "TRACKER-N/A";

        bool shouldSleep = false;
        Logging::Logger *logger;
        GPS_TRACKER::SIM7000G *sim;
        GPS_TRACKER::Configuration *configuration;
        GPS_TRACKER::StateManager *stateManager;
        AudioPlayer::Player *audioPlayer;
        AudioOutputI2S audioOutput;
        AudioGeneratorMP3 mp3;
        AudioFileSourceSPIFFS source;
        Timestamp lastUpdate;
    };
}

#endif //LIGHTWEIGHT_GPS_TRACKER_TRACKER_H

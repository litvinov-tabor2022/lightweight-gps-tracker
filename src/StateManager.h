#ifndef LIGHTWEIGHT_GPS_TRACKER_STATEMANAGER_H
#define LIGHTWEIGHT_GPS_TRACKER_STATEMANAGER_H

#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <cstring>
#include <cmath>
#include <utility>
#include <functional>
#include "Protocol.h"
#include "Configuration.h"

namespace MQTT {
    enum STATE {
        RECONNECTING, CONNECTED, DISCONNECTED
    };
}
namespace GSM {
    enum STATE {
        RECONNECTING, CONNECTED, DISCONNECTED
    };
}

namespace AudioPlayer {
    enum STATE {
        PLAYING, STOPPED
    };
}

namespace GPS_TRACKER {
    class StateManager {
    public:
        explicit StateManager(Configuration *configuration);

        /**
         * Call after the SPIFFS is initialized.
         * */
        void begin();

        void test();

        static void removePersistedState();

        [[nodiscard]] MQTT::STATE getMqttState() const;

        [[nodiscard]] GSM::STATE getGsmState() const;

        [[nodiscard]] size_t getVisitedWaypoints() const;

        void setMqttState(MQTT::STATE mqttState);

        void setGsmState(GSM::STATE state);

        void enqueuePosition(const GPS_TRACKER::GPSCoordinates& newPosition);

        void updatePosition(GPS_TRACKER::GPSCoordinates newPosition, bool shouldBeEnqueued = true);

        void onReachedWaypoint(std::function<void(const waypoint &)> callback);

        [[nodiscard]] GPS_TRACKER::Timestamp getLastFastFixFileUpdate() const;

        [[nodiscard]] bool isFastFixFileValid(GPS_TRACKER::Timestamp timestamp) const;

        void setTimeOfLastReset(GPS_TRACKER::Timestamp timestamp);

        void setLastFastFixFileUpdate(GPS_TRACKER::Timestamp lastFastFixFileUpdate);

        [[nodiscard]] esp_sleep_wakeup_cause_t getWakeupReason() const;

        double distanceToNextWaypoint();

        void setNumberOfConnectedDevices(uint8_t no);

        bool couldSleep();

        [[nodiscard]] bool shouldBeRestarted() const;

        void updateActTime(GPS_TRACKER::Timestamp timestamp);

        void needsRestart();

        bool getPositionFromBuffer(GPSCoordinates &coordinates);
    private:
        void checkCollision();

        static double deg2rad(float deg);

        void loadPersistState();

        void persistState();

        static void clearMemory();

        void serialize(JsonDocument *doc) const;

        void deserialize(JsonDocument &doc);

        static inline String stateFile = "/state.json";

        bool isRestartNeeded = false;
        unsigned long lastFastFixFileUpdate = 0;
        AudioPlayer::STATE audioPlayerState = AudioPlayer::STOPPED;
        MQTT::STATE mqttState = MQTT::DISCONNECTED;
        GSM::STATE gsmState = GSM::DISCONNECTED;

        int bufferSizeLimit = 10;
        size_t visitedWaypoints = 0;
        std::function<void(const waypoint &)> newWaypointReachedCallback;
        GPS_TRACKER::GPSCoordinates actPosition;
        Configuration *configuration;
        esp_sleep_wakeup_cause_t wakeup_reason;
        uint8_t connectedDevices = 0;
        GPS_TRACKER::Timestamp timeOfLastReset;
        GPS_TRACKER::Timestamp actTime;
        std::deque<GPSCoordinates> coordinatesBuffer;
    };
}

#endif //LIGHTWEIGHT_GPS_TRACKER_STATEMANAGER_H

#include <Arduino.h>
#include "Configuration.h"
#include "sim/SIM868.h"
#include "SPIFFS.h"
#include "audio/Player.h"
#include "Protocol.h"
#include "AudioFileSourceSPIFFS.h"

GPS_TRACKER::SIM868 *sim;
GPS_TRACKER::Configuration *configuration;
GPS_TRACKER::StateManager *stateManager;
AudioPlayer::Player *audioPlayer;
AudioOutputI2S out;
AudioGeneratorMP3 mp3;
AudioFileSourceSPIFFS source;

void setup() {
    Serial.begin(115200);
    Serial.println("Initialization start....");
    if (!SPIFFS.begin()) {
        Serial.println("SPIFFS init failed.");
    }

    pinMode(0, PULLUP);
    DefaultTasker.loopEvery("", 250, [] {
        if (!digitalRead(0)) {
            Serial.println("Deleting persisted state");
            GPS_TRACKER::StateManager::removePersistedState();
        }
    });

    // ------ CONFIGURATION
    configuration = new GPS_TRACKER::Configuration();
    if (!configuration->read()) {
        Serial.println("GPS TRACKER INITIALIZATION FAILED!!!!!!!!");
    } else {
        Serial.printf("Configuration loaded.\n\t # waypoints: %d\n", configuration->WAYPOINTS.size());
    }

    stateManager = new GPS_TRACKER::StateManager(configuration);
    stateManager->begin();

    // ------ GSM/GPS
    sim = new GPS_TRACKER::SIM868(*configuration);
    MODEM::ISIM::STATUS_CODE initRes = sim->init();
    if (initRes != MODEM::ISIM::Ok) {
        Serial.printf("Modem initialization failed> %u\n", initRes);
    }

    // ------ AUDIO
    audioPlayer = new AudioPlayer::Player(&mp3, &out, &source, (DEFAULT_VOLUME / 100.0));
    audioPlayer->setVolume(DEFAULT_VOLUME);
//    audioPlayer->enqueueFile("/moses.mp3");
    audioPlayer->play();

    stateManager->onReachedWaypoint([](const GPS_TRACKER::waypoint &w) {
        audioPlayer->enqueueFile(w.path);
    });

    // ------
    DefaultTasker.loopEvery("sender", 2000, [] {
        GPS_TRACKER::GPSCoordinates coordinates;
        MODEM::ISIM::STATUS_CODE actPositionState = sim->actualPosition(&coordinates);
        if (MODEM::ISIM::Ok != actPositionState) {
            Serial.printf("Position is not valid, skipping: %d\n", actPositionState);
            return;
        }
        stateManager->updatePosition(coordinates);
        GPS_TRACKER::Message message(configuration->CONFIG.trackerId, stateManager->getVisitedWaypoints(), coordinates);
        std::string serializeMessage;
        if (!message.serialize(serializeMessage)) {
            Serial.println("Message serialization error");
        }
        Serial.printf("Act coords: %s\n", serializeMessage.c_str());
        Serial.println("Reading coords");
        sim->sendData(serializeMessage);
    });
}

void loop() {
    // everything is handled by tasks
}
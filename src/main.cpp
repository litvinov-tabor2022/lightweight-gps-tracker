#include <Arduino.h>
#include "Configuration.h"
//#include "sim/SIM868.h"
#include "sim/SIM7000G.h"
#include "SPIFFS.h"
#include "audio/Player.h"
#include "AudioFileSourceSPIFFS.h"

using namespace Logging;
using namespace GPS_TRACKER;
using namespace MODEM;

Logger *logger;
SIM7000G *sim;
Configuration *configuration;
StateManager *stateManager;
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

    // ------ CONFIGURATION
    configuration = new Configuration();
    if (!configuration->read()) {
        Serial.println("GPS TRACKER INITIALIZATION FAILED!!!!!!!!");
    } else {
        Serial.printf("Configuration loaded.\n\t # waypoints: %d\n", configuration->WAYPOINTS.size());
    }

    // ------ LOGGER (Uncomment following if you want to use Telnet logging)
//    String trackerSSID = "TRACKER-" + (String) configuration->CONFIG.trackerId;
//    WiFi.mode(WIFI_AP);
//    WiFi.softAP(trackerSSID.c_str(), "tracker123");
//
//    IPAddress myIP = WiFi.softAPIP();
//    Serial.println();
//    Serial.print("AP IP address: ");
//    Serial.println(myIP);

// and change logger to Logger::serialAndTelnetLogger(...)
    logger = Logger::serialLogger(DEBUG);

    // ------ STATE
    stateManager = new StateManager(configuration);
    stateManager->begin();

    // ------ GSM/GPS
    sim = new SIM7000G(logger, *configuration, stateManager);
    STATUS_CODE initRes = sim->init();
    if (initRes != Ok) {
        logger->printf(ERROR, "Modem initialization failed with code: %u\n", initRes);
    }

    // ------ AUDIO
    audioPlayer = new AudioPlayer::Player(logger, &mp3, &out, &source, (DEFAULT_VOLUME / 100.0));
    audioPlayer->setVolume(DEFAULT_VOLUME);
    audioPlayer->play();

    stateManager->onReachedWaypoint([](const waypoint &w) {
        // Sometimes the communication with SIM module is wierd after playing sound, so this should help.
//        sim->powerOff();
        logger->printf(INFO, "Waypoint no. %d was reached\n", w.id);
        audioPlayer->enqueueFile(w.path);
    });

    esp_sleep_enable_timer_wakeup(configuration->CONFIG.sleepTime * uS_TO_S_FACTOR);
}

bool shouldSleep = false;

void loop() {
    STATUS_CODE res = sim->sendActPosition();
    switch (res) {
        case Ok:
            if (stateManager->distanceToNextWaypoint() < 150) { // don't sleep if the waypoint is close
                delay(500);
            } else {
                shouldSleep = true;
            }
            break;
        case GPS_CONNECTION_ERROR: // fatal error
            esp_restart();
        default:
            delay(10);
            break;
    }

    if (!audioPlayer->playing() && shouldSleep) {
//        sim->sleep(); // This is not necessary (now), battery lifetime without sleeping SIM module is good enough
        logger->println(INFO, "Going to sleep");
        logger->flush();
        delay(50);
        esp_deep_sleep_start();
    }
}
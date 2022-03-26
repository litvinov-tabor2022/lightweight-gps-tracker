#include <Arduino.h>
#include "Configuration.h"
//#include "sim/SIM868.h"
#include "sim/SIM7000G.h"
#include "SPIFFS.h"
#include "audio/Player.h"
#include "Protocol.h"
#include "AudioFileSourceSPIFFS.h"

using namespace Logging;
using namespace GPS_TRACKER;
using namespace MODEM;

IPAddress IP = {10, 10, 1, 1};
IPAddress gateway = {10, 10, 1, 1};
IPAddress NMask = {255, 255, 255, 0};

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
    pinMode(PWR_PIN, OUTPUT);
    digitalWrite(PWR_PIN, HIGH);
    delay(300);
    digitalWrite(PWR_PIN, LOW);

    Serial.println("Initialization start....");
    if (!SPIFFS.begin()) {
        Serial.println("SPIFFS init failed.");
    }
    pinMode(0, PULLUP);


    DefaultTasker.loopEvery("", 250, [] {
        if (!digitalRead(0)) {
            Serial.println("Deleting persisted state");
            StateManager::removePersistedState();
        }
    });

    // ------ CONFIGURATION
    configuration = new Configuration();
    if (!configuration->read()) {
        Serial.println("GPS TRACKER INITIALIZATION FAILED!!!!!!!!");
    } else {
        Serial.printf("Configuration loaded.\n\t # waypoints: %d\n", configuration->WAYPOINTS.size());
    }

    stateManager = new StateManager(configuration);
    stateManager->begin();

    String trackerSSID = "TRACKER-" + (String) configuration->CONFIG.trackerId;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(trackerSSID.c_str(), "tracker123");
    WiFi.softAPConfig(IP, IP, NMask);

    IPAddress myIP = WiFi.softAPIP();
    Serial.println();
    Serial.print("AP IP address: ");
    Serial.println(myIP);

    logger = Logger::serialAndTelnetLogger(Logging::DEBUG);

    // ------ GSM/GPS
    sim = new SIM7000G(logger, *configuration);
    STATUS_CODE initRes = sim->init();
    if (initRes != Ok) {
        Serial.printf("Modem initialization failed> %u\n", initRes);
    }

    // ------ AUDIO
    audioPlayer = new AudioPlayer::Player(&mp3, &out, &source, (DEFAULT_VOLUME / 100.0));
    audioPlayer->setVolume(DEFAULT_VOLUME);
//    audioPlayer->enqueueFile("/moses.mp3");
    audioPlayer->play();

    stateManager->onReachedWaypoint([](const waypoint &w) {
        audioPlayer->enqueueFile(w.path);
    });

    // ------
    DefaultTasker.loopEvery("sender", 2000, [] {
        GPSCoordinates coordinates;
        STATUS_CODE actPositionState = sim->actualPosition(&coordinates);
        if (Ok != actPositionState) {
            Serial.printf("Position is not valid, skipping: %d\n", actPositionState);
            return;
        }
        stateManager->updatePosition(coordinates);
        Message message(configuration->CONFIG.trackerId, stateManager->getVisitedWaypoints(), coordinates);
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
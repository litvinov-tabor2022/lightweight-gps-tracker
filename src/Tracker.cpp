#include "Tracker.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

bool GPS_TRACKER::Tracker::begin() {
    initLogger();

    audioOutput = new AudioOutputI2S();
    mp3 = new AudioGeneratorMP3();
    source = new AudioFileSourceSPIFFS();
    Serial.println("Initialization start....");

    initSPIFFS();
    initConfiguration();

    initStateManager();
    initModem();
    initAudio();

    esp_sleep_enable_timer_wakeup(configuration->CONFIG.sleepTime * uS_TO_S_FACTOR);
    registerOnReachedWaypoint();
    trackerLoop();

    return true;
}

void GPS_TRACKER::Tracker::initLogger() {
    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    if (SD.begin(SD_CS)) {
        Serial.println("SD logger");
        loggerFile = SD.open("/tracker.log", FILE_APPEND);
        logger = Logging::Logger::fileLogger(&loggerFile, Logging::DEBUG);
    } else {
        Serial.println("Serial logger");
        logger = Logging::Logger::serialLogger(Logging::DEBUG);
    }
    logger->println(Logging::INFO, "Logger initialized");
}

void GPS_TRACKER::Tracker::initPins() {
    return;
}

void GPS_TRACKER::Tracker::trackerLoop() {
    // TODO: send position less times when audio is playing (or this loop is iterate more than once)
    DefaultTasker.loopEvery("loop", 500, [this] {
        GPS_TRACKER::STATUS_CODE res = sim->sendActPosition();
        switch (res) {
            case GPS_TRACKER::Ok:
                if (stateManager->distanceToNextWaypoint() > 150) { // don't sleep if the waypoint is close
                    shouldSleep = true;
                }
                break;
            case GPS_TRACKER::GPS_CONNECTION_ERROR: // fatal error
                while (!audioPlayer->playing()) {
                    Tasker::sleep(100);
                }
                esp_restart();
            default:
                // noop
                break;
        }

        if (!audioPlayer->playing() && shouldSleep) {
            digitalWrite(12, HIGH); // turn off led
            sim->sleep(); // This is not necessary (now), battery lifetime without sleeping SIM module is good enough
            logger->println(Logging::INFO, "Going to sleep");
            esp_light_sleep_start();
            shouldSleep = false;
        }
    });
}

void GPS_TRACKER::Tracker::registerOnReachedWaypoint() {
    stateManager->onReachedWaypoint([this](const GPS_TRACKER::waypoint &w) {
        // Sometimes the communication with SIM module is wierd after playing sound, so this should help.
//        sim->powerOff();
        logger->printf(Logging::INFO, "Waypoint no. %d was reached\n", w.id);
        audioPlayer->enqueueFile(w.path);
    });
}

bool GPS_TRACKER::Tracker::init() {
    initPins();
    return true;
}

void GPS_TRACKER::Tracker::initAudio() {
// ------ AUDIO
    audioOutput->SetPinout(22, 21, 23);
    audioPlayer = new AudioPlayer::Player(logger, mp3, audioOutput, source, (DEFAULT_VOLUME / 100.0));
    audioPlayer->setVolume(DEFAULT_VOLUME);
//    audioPlayer->enqueueFile("/moses.mp3");
    audioPlayer->play();
}

void GPS_TRACKER::Tracker::initStateManager() {
// ------ STATE
    stateManager = new GPS_TRACKER::StateManager(configuration);
    stateManager->begin();
}

void GPS_TRACKER::Tracker::initModem() {
    // ------ GSM/GPS
    sim = new GPS_TRACKER::SIM7000G(logger, *configuration, stateManager);
    GPS_TRACKER::STATUS_CODE initRes = sim->init();
    if (initRes != GPS_TRACKER::Ok) {
        logger->printf(Logging::ERROR, "Modem initialization failed with code: %u\n", initRes);
    }
}

void GPS_TRACKER::Tracker::initConfiguration() {
// ------ CONFIGURATION
    configuration = new GPS_TRACKER::Configuration();
    if (!configuration->read()) {
        Serial.println("GPS TRACKER INITIALIZATION FAILED!!!!!!!!");
    } else {
        Serial.printf("Configuration loaded.\n\t # waypoints: %d\n", configuration->WAYPOINTS.size());
    }
}

bool GPS_TRACKER::Tracker::initSPIFFS() {
    if (!SPIFFS.begin()) {
        Serial.println("SPIFFS init failed.");
        return false;
    }
    return true;
}

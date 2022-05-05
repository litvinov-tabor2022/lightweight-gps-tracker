#include "Tracker.h"
#include <Arduino.h>

bool GPS_TRACKER::Tracker::begin() {
    initLogger();

    logger->println(Logging::INFO, "Initialization start....");

    if (!initSPIFFS()) {
        return false;
    }
    if (!initConfiguration()) {
        return false;
    }
    initStateManager();
    if (!initModem()) {
        return false;
    }
    initAudio();

    logger->printf(Logging::INFO, "Sleep time %d\n", configuration->CONFIG.sleepTime);

    esp_sleep_enable_timer_wakeup(configuration->CONFIG.sleepTime * uS_TO_S_FACTOR);
    registerOnReachedWaypoint();
    trackerLoop();

    return true;
}

void GPS_TRACKER::Tracker::initLogger() {
    Serial.println("Serial logger");
    logger = Logging::Logger::serialLogger(Logging::DEBUG);
    logger->println(Logging::INFO, "Logger initialized");
}

void GPS_TRACKER::Tracker::initPins() {
    pinMode(LED_PIN, OUTPUT);
}

void GPS_TRACKER::Tracker::trackerLoop() {
    // TODO: send position less times when audio is playing (or this loop is iterate more than once)
    DefaultTasker.loopEvery("loop", 500, [&] {
        digitalWrite(LED_PIN, LOW); // turn led on
        GPS_TRACKER::STATUS_CODE res = sim->sendActPosition();
        switch (res) {
            case GPS_TRACKER::GPS_ACCURACY_TOO_LOW:
                logger->printf(Logging::ERROR, "Accuracy is too low", res);
                break;
            case GPS_TRACKER::Ok: {
                double distance = stateManager->distanceToNextWaypoint();
                logger->printf(Logging::INFO, "Distance from next waypoint is: %f\n", distance);
                if (distance > 150) { // don't sleep if the waypoint is close
                    shouldSleep = true;
                } else {
                    logger->println(Logging::INFO,
                                    "Distance from next waypoint is too short, sleeping will be skipped");
                }
                break;
            }
            case GPS_TRACKER::SENDING_DATA_FAILED:
                logger->println(Logging::ERROR, "Sending actual position to MQTT failed");
                shouldSleep = false;
                break;
            case GPS_TRACKER::SERIALIZATION_ERROR:
                logger->println(Logging::ERROR, "Serialization error");
                break;
            default:
                logger->printf(Logging::ERROR, "Unknown error, tracker needs to be restarted. (cause : %d)\n", res);
                while (audioPlayer->playing()) {
                    Tasker::sleep(100);
                }
                delay(100);
                // TODO (un)comment?
                esp_restart();
                break;
        }

        if (!audioPlayer->playing() && shouldSleep) {
            digitalWrite(LED_PIN, HIGH); // turn off led
            sim->sleep(); // This is not necessary (now), battery lifetime without sleeping SIM module is good enough
            logger->println(Logging::INFO, "Going to sleep");
            delay(100);
            esp_light_sleep_start();
            shouldSleep = false;
            logger->println(Logging::INFO, "Wake up");
        }
    });
}

void GPS_TRACKER::Tracker::registerOnReachedWaypoint() {
    stateManager->onReachedWaypoint([&](const GPS_TRACKER::waypoint &w) {
//        sim->powerOff();
        logger->printf(Logging::INFO, "Waypoint no. %d was reached\n", w.id);
        audioPlayer->enqueueFile(w.path);
    });
    logger->println(Logging::INFO, "OnReachedWaypoint callback registered");
}

bool GPS_TRACKER::Tracker::init() {
    initPins();
    return true;
}

void GPS_TRACKER::Tracker::initAudio() {
// ------ AUDIO
    audioOutput.SetPinout(22, 21, 23);
    audioPlayer = new AudioPlayer::Player(logger, &mp3, &audioOutput, &source, (DEFAULT_VOLUME / 100.0));
    audioPlayer->setVolume(DEFAULT_VOLUME);
    audioPlayer->play();
    logger->println(Logging::INFO, "Audio module initialized");
}

void GPS_TRACKER::Tracker::initStateManager() {
// ------ STATE
    stateManager = new GPS_TRACKER::StateManager(configuration);
    stateManager->begin();
    logger->println(Logging::INFO, "State manager initialized");
}

bool GPS_TRACKER::Tracker::initModem() {
    // ------ GSM/GPS
    sim = new GPS_TRACKER::SIM7000G(logger, *configuration, stateManager);
    GPS_TRACKER::STATUS_CODE initRes = sim->init();
    if (initRes != GPS_TRACKER::Ok) {
        logger->printf(Logging::ERROR, "Modem initialization failed with code: %u\n", initRes);
        return false;
    } else {
        logger->println(Logging::INFO, "Modem initialized");
        return true;
    }
}

bool GPS_TRACKER::Tracker::initConfiguration() {
// ------ CONFIGURATION
    configuration = new GPS_TRACKER::Configuration();
    if (!configuration->read()) {
        logger->println(Logging::ERROR, "GPS TRACKER INITIALIZATION FAILED");
        return false;
    } else {
        logger->printf(Logging::INFO, "Configuration loaded.\n\t # waypoints: %d\n", configuration->WAYPOINTS.size());
        return true;
    }
}

bool GPS_TRACKER::Tracker::initSPIFFS() {
    if (!SPIFFS.begin()) {
        logger->println(Logging::ERROR, "SPIFFS init failed.");
        return false;
    }
    return true;
}

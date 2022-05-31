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
    auto timestamp = sim->getActTime();
    stateManager->setTimeOfLastReset(timestamp.value_or(0));
    stateManager->updateActTime(timestamp.value_or(0));

    initAudio();

    logger->printf(Logging::INFO, "Sleep time %d\n", configuration->CONFIG.sleepTime);

    esp_sleep_enable_timer_wakeup(configuration->CONFIG.sleepTime * uS_TO_S_FACTOR);
    registerOnReachedWaypoint();
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
    sim->wakeUp();
    digitalWrite(LED_PIN, LOW); // turn led on
    logger->println(Logging::DEBUG, "Reading act time");
    auto actTime = sim->getActTime();
    if (actTime.has_value()) {
        logger->println(Logging::DEBUG, "Updating time");
        stateManager->updateActTime(actTime.value());
    }
    if (millis() - lastUpdate < 25000) {
        logger->println(Logging::DEBUG, "Fast calling. Skipping tracker loop.");
        return;
    }
    lastUpdate = millis();
    GPS_TRACKER::STATUS_CODE res = UNKNOWN_ERROR;

    // read GPS position
    int errorAttempts = 0;
    while (res != Ok) {
        // too much unsuccessful reads of GPS position
        if (errorAttempts > 3) {
            logger->println(Logging::ERROR, "Reading GPS position failed more times.");
            stateManager->needsRestart();
            break;
        }
        GPSCoordinates actPosition;
        logger->println(Logging::DEBUG, "Reading act position");
        res = sim->actualPosition(&actPosition);
        switch (res) {
            case Ok:
                logger->println(Logging::DEBUG, "Updating and enqueuing position");
                // insert position to read buffer
                stateManager->updatePosition(actPosition);
                break;
            case GPS_ACCURACY_TOO_LOW:
                logger->printf(Logging::WARNING,
                               "Accuracy is too low. Acc value: %d. This is non critical and can be cause by pour signal quality.",
                               res);
                break;
            case GPS_CONNECTION_ERROR:
                logger->println(Logging::ERROR, "GPS connection failed");
                break;
            case READ_GPS_COORDINATES_FAILED:
                logger->println(Logging::ERROR, "Reading GPS coordinates failed. Check antenna connection.");
                break;
            case GPS_COORDINATES_OUT_OF_RANGE:
                logger->printf(Logging::ERROR, "Coordinates out of range. lat: %l lon: %l\n", actPosition.lat,
                               actPosition.lon);
                break;
            default:
                logger->println(Logging::ERROR, "Unknown GPS error :-(");
        }

        // reset will not help with accuracy of GPS position
        if (res != GPS_ACCURACY_TOO_LOW && res != Ok) {
            errorAttempts++;
            delay(250);
        }
    }

    // send GPS position(s)
    int positionCounter = 0;
    GPSCoordinates positionToSend;
    while (stateManager->getPositionFromBuffer(positionToSend)) {
        logger->printf(Logging::INFO, "Sending position %d from buffer\n", positionCounter++);
        GPS_TRACKER::STATUS_CODE sendStatus = sim->sendPosition(positionToSend);
        if (sendStatus != Ok) { // sending failed (try it in next iteration)
            logger->println(Logging::WARNING, "Sending position failed.");
            stateManager->enqueuePosition(positionToSend);
            break;
        }
    }

    if (stateManager->shouldBeRestarted()) {
        logger->println(Logging::WARNING, "IN FEW MOMENTS, TRACKER WILL BE RESTARTED");
        restartTracker();
    }

    if (stateManager->couldSleep()) {
        sleepTracker();
    } else {
        logger->println(Logging::INFO, "Skipping sleep");
    }
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

void Tracker::restartTracker() {
    logger->println(Logging::INFO, "Restarting tracker");
    while (audioPlayer->playing()) {
        Tasker::sleep(100);
    }
    esp_restart();
}

void Tracker::sleepTracker() {
    while (audioPlayer->playing()) {
        logger->println(Logging::INFO, "Waiting to end of playback");
        Tasker::sleep(100);
    }
    digitalWrite(LED_PIN, HIGH); // turn off led
    sim->sleep(); // This is not necessary (now), battery lifetime without sleeping SIM module is good enough
    logger->println(Logging::INFO, "Going to sleep");
    delay(100);
    esp_light_sleep_start();
}

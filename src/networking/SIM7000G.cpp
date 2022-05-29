#include "SIM7000G.h"
#include "HwLocks.h"

MODEM::STATUS_CODE GPS_TRACKER::SIM7000G::init() {
    logger->println(Logging::INFO, "Initializing SIM700G module...");

    if (configuration.GSM_CONFIG.enable) {
        logger->println(Logging::INFO, "Connecting to GSM/MQTT");
        if (connectGPRS()) {
            mqttClient.init(configuration, logger, &gsmClientSSL);
            if (!mqttClient.begin()) logger->println(Logging::WARNING, "Connecting to MQTT failed");;
        } else {
            logger->println(Logging::INFO, "Connecting to GSM failed");
        }
    }

    if (configuration.GPS_CONFIG.enable) {
        logger->println(Logging::INFO, "Enabling GPS (waiting for first fix)");
        int failedConnection = 0;
        bool isGPSConnected = false;
        while (failedConnection < 4) {
            if (!connectGPS()) failedConnection++;
            else {
                isGPSConnected = true;
                break;
            }
        }
        if (!isGPSConnected) return GPS_CONNECTION_ERROR;
        else logger->println(Logging::INFO, "Modem connected to GPS");
    }
    return Ok;
}

MODEM::STATUS_CODE GPS_TRACKER::SIM7000G::actualPosition(GPSCoordinates *coordinates) {
    float lat, lon, speed, alt, accuracy;
    int vsat, usat;
    tm rawTime{};
    if (modem.getGPS(&lat, &lon, &speed, &alt, &vsat, &usat, &accuracy, &rawTime.tm_year, &rawTime.tm_mon,
                     &rawTime.tm_mday, &rawTime.tm_hour, &rawTime.tm_min, &rawTime.tm_sec)) {
        // Position is out of range
        if (abs(lat) > 90 || abs(lon) > 180) {
            logger->println(Logging::WARNING, "Invalid position read");
            return GPS_COORDINATES_OUT_OF_RANGE;
        }

        rawTime.tm_year -= 1900;
        rawTime.tm_mon -= 1;
        long timestamp = mktime(&rawTime);
        logger->printf(Logging::INFO, "lat: %f, lon: %f, alt: %f, acc: %f, timestamp: %ld\n", lat, lon, alt, accuracy,
                       timestamp);

        // Accuracy is below the minimal threshold
        if (accuracy > configuration.GPS_CONFIG.minimal_accuracy) {
            logger->printf(Logging::WARNING, "Accuracy is too low: %f < %f\n", accuracy,
                           configuration.GPS_CONFIG.minimal_accuracy);
            return GPS_ACCURACY_TOO_LOW;
        }

        *coordinates = GPSCoordinates(lat, lon, alt, timestamp);

        return Ok;
    }

    return connectGPS() ? READ_GPS_COORDINATES_FAILED : MODEM_NOT_CONNECTED;
}

bool GPS_TRACKER::SIM7000G::isConnected() {
    return modem.testAT(5000);
}

bool GPS_TRACKER::SIM7000G::isNetworkConnected() {
    logger->printf(Logging::INFO, "is network connected: %b, is gprs connected %b\n", modem.isNetworkConnected(),
                   modem.isGprsConnected());
    return isConnected() && modem.isNetworkConnected() && modem.isGprsConnected();
}

bool GPS_TRACKER::SIM7000G::connectGPRS() {

    wakeUp();
    SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

//    if (stateManager->getWakeupReason() != ESP_SLEEP_WAKEUP_TIMER) {
//        if (!modem.restart()) {
//            logger->println(Logging::WARNING, "Failed to restart modem.");
//        }
//    }

    if (isNetworkConnected()) {
        updateTime();
        return true;
    }

    logger->printf(Logging::INFO, "Modem: %s\n", modem.getModemInfo().c_str());

    /*
      2 Automatic
      13 GSM only
      38 LTE only
      51 GSM and LTE only
    * * * */
    String res;
    res = modem.setNetworkMode(13);
    if (!res) {
        logger->println(Logging::ERROR, "setNetworkMode failed");
        return false;
    }

    /*
      1 CAT-M
      2 NB-Iot
      3 CAT-M and NB-IoT
    * * */
    res = modem.setPreferredMode(1);
    if (!res) {
        logger->println(Logging::ERROR, "setPreferredMode failed");
        return false;
    }

    bool modemConnectedSuccessfully = false;
    int failedAttempts = 0;
    while (!modemConnectedSuccessfully) {
        if (failedAttempts > 1) {
            logger->println(Logging::ERROR, "Connect to network failed! Hard reset may be needed.");
            return false;
        }
        logger->println(Logging::INFO, "Waiting for network...");
        if (!modem.waitForNetwork(5000)) { // 5s timeout
            failedAttempts++;
            logger->println(Logging::ERROR, " failed!");
            continue;
        }

        logger->printf(Logging::INFO, "Connecting to %s\n", configuration.GSM_CONFIG.apn.c_str());
        if (!modem.gprsConnect(configuration.GSM_CONFIG.apn.c_str(),
                               configuration.GSM_CONFIG.user.c_str(),
                               configuration.GSM_CONFIG.password.c_str())) {
            logger->println(Logging::ERROR, " failed!");
            continue;
        }
        logger->println(Logging::INFO, " succeed!");
        modemConnectedSuccessfully = true;
    }
    updateTime();
    return true;
}

bool GPS_TRACKER::SIM7000G::connectGPS() {

    wakeUp();

    modem.sendAT("+SGPIO=0,4,1,1");
    if (modem.waitResponse(10000L) != 1) {
        logger->println(Logging::ERROR, " SGPIO=0,4,1,1 false");
        return false;
    }

    if (!modem.enableGPS()) {
        logger->println(Logging::ERROR, "Enabling GPS failed");
        return false;
    }
    auto actTime = getActTime();
    if (actTime.has_value()) {
        // update file for fast fix once for 2.5 days
        logger->printf(Logging::INFO, "Time since last XTRA file update: %s\n",
                       stateManager->getLastFastFixFileUpdate() - actTime.value());
        if (configuration.GPS_CONFIG.fastFix && stateManager->isFastFixFileValid(actTime.value())) {
            fastFix();
            stateManager->setLastFastFixFileUpdate(actTime.value());
            coldStart();
        } else {
            logger->println(Logging::INFO, "Skipping downloading of XTRA file");
            hotStart();
        }
    } else {
        logger->println(Logging::INFO, "Network time is not available, skipping XTRA file (TTF can be long)");
        coldStart();
    }

    float lat, lon, speed, alt, accuracy;
    int vsat, usat;
    logger->println(Logging::INFO, "Waiting for GPS");
    int failedAttempts = 0;
    while (!modem.getGPS(&lat, &lon, &speed, &alt, &vsat, &usat, &accuracy)) {
        failedAttempts++;
        if (failedAttempts > 20     // too many attempts
            && actTime.has_value()  // and the module is ready to hot start
            && stateManager->isFastFixFileValid(actTime.value())) {
            return false;
        }

        if (modem.testAT()) {
            logger->printf(Logging::DEBUG, "%s, attempt no. %d", modem.getGPSraw(), failedAttempts);
//            logger->printf(Logging::DEBUG, "vsat: %d, usat: %d\n", vsat, usat);
            Tasker::sleep(1500);
        } else {
            // module disconnected
            return false;
        }
    }

    logger->println(Logging::INFO, "GPS successfully enabled");
    return true;
}

bool GPS_TRACKER::SIM7000G::isGpsConnected() {

    float lat, lon;
    return modem.getGPS(&lat, &lon);
}

bool GPS_TRACKER::SIM7000G::reconnectGPS() {
    logger->println(Logging::INFO, "Reconnecting GPS");
    if (!isGpsConnected()) {
        logger->println(Logging::INFO, "Reconnecting GPS");
        if (!connectGPS()) {
            stateManager->needsRestart();
            return false;
        }
    } else {
        Serial.println("GPS modem connected");
    }
    return true;
}

bool GPS_TRACKER::SIM7000G::reconnectGSM() {
    logger->printf(Logging::DEBUG, "Time since last reconnect: %d\n", millis() - lastGsmReconnect);
    if ((millis() - lastGsmReconnect) < (60 * mS_TO_S_FACTOR)) {
        logger->println(Logging::DEBUG, "Skipping GSM reconnect routine");
        return false;
    }
    if (!isNetworkConnected()) {
        lastGsmReconnect = millis();
        logger->println(Logging::INFO, "Reconnecting GPRS modem");
        if (!connectGPRS()) {
            modem.sleepEnable(false);
            modem.poweroff();
            return false;
        } else {
            // MQTT reconnect must be called even if the modem lost internet connection
            if (!mqttClient.reconnect(1)) return false;
        }
    } else {
        logger->println(Logging::INFO, "GSM modem connected");
    }
    if (!mqttClient.isConnected()) {
        logger->println(Logging::INFO, "Reconnecting MQTT client");
        if (!mqttClient.reconnect(1)) {
            stateManager->needsRestart();
            return false;
        }
    } else {
        logger->println(Logging::INFO, "MQTT modem connected");
    }
    return true;
}

//bool GPS_TRACKER::SIM7000G::reconnect() {
//    logger->println(Logging::INFO, "Reconnecting");
//    while (!isNetworkConnected() || !mqttClient.isConnected() || !isGpsConnected()) {
//        wakeUp();
//        if (!isNetworkConnected()) {
//            logger->println(Logging::INFO, "Reconnecting GPRS modem");
//            if (!connectGPRS()) {
//                modem.sleepEnable(false);
//                modem.poweroff();
//                stateManager->needsRestart();
//                return false;
//            } else {
//                gsmClientSSL = SSLClient(&gsmClient); // reset SSL
//                // MQTT reconnect must be called even if the modem lost internet connection
//                if (!mqttClient.reconnect(1)) return false;
//            }
//        } else {
//            logger->println(Logging::INFO, "GSM modem connected");
//        }
//        if (!isGpsConnected()) {
//            logger->println(Logging::INFO, "Reconnecting GPS");
//            if (!connectGPS()) {
//                stateManager->needsRestart();
//                return false;
//            }
//        } else {
//            Serial.println("GPS modem connected");
//        }
//        if (!mqttClient.isConnected()) {
//            logger->println(Logging::INFO, "Reconnecting MQTT client");
//            if (!mqttClient.reconnect(1)) {
//                stateManager->needsRestart();
//                return false;
//            }
//        } else {
//            logger->println(Logging::INFO, "MQTT modem connected");
//        }
//    }
//    logger->println(Logging::INFO, "Reconnecting done");
//    return true;
//}

void GPS_TRACKER::SIM7000G::fastFix() {

    modem.sendAT(GF("+CGNSMOD=1,1,1,1"));
    modem.waitResponse();
    std::string cmd = "+SAPBR=3,1, \"APN\",\"" + configuration.GSM_CONFIG.apn + "\"";
    modem.sendAT(GF(cmd.c_str()));
    modem.waitResponse();
    modem.sendAT(GF("+SAPBR=1,1"));
    modem.waitResponse();
    modem.sendAT(GF("+CNTPCID=1"));
    modem.waitResponse();
    modem.sendAT(GF("+CNTP=\"ntp1.aliyun.com\",32,1"));
    modem.waitResponse();
    modem.sendAT(GF("+CNTP"));
//    modem.waitResponse();
    modem.waitResponse(5000L, GF("+CNTP: 1"));
    modem.sendAT(GF("+CNACT=1,\"internet.t-mobile.cz\""));
    modem.waitResponse();
    modem.waitResponse(5000L, GF("+APP PDP: ACTIVE"));
    modem.sendAT(GF("+HTTPTOFS=\"http://xtrapath1.izatcloud.net/xtra3grc.bin\",\"/customer/xtra3grc.bin\""));
    modem.waitResponse(5000L, GF("+HTTPTOFS:"));
    modem.sendAT(GF("+CGNSCPY"));
    modem.waitResponse();
    modem.sendAT(GF("+CGNSXTRA=1"));
    modem.waitResponse();
}

void GPS_TRACKER::SIM7000G::hotStart() {
    logger->println(Logging::INFO, "GPS hot start");
    modem.sendAT(GF("+CGNSHOT"));
    modem.waitResponse();
}

void GPS_TRACKER::SIM7000G::warmStart() {
    logger->println(Logging::INFO, "GPS warm start");
    modem.sendAT(GF("+CGNSWARM"));
    modem.waitResponse();
}

void GPS_TRACKER::SIM7000G::coldStart() {
    logger->println(Logging::INFO, "GPS cold start");
    modem.sendAT(GF("+CGNSCOLD"));
    modem.waitResponse(5000L, GF("+CGNSXTRA: 0"), GF("+CGNSXTRA: 2"));
}

void GPS_TRACKER::SIM7000G::setGPSAccuracy(int meters = 50) {
    String cmd = "+CGNSHOR=" + (String) meters;
    modem.sendAT(GF(cmd));
    modem.waitResponse();
}

std::optional<Timestamp> GPS_TRACKER::SIM7000G::getActTime() {
    float timezone;
    tm rawTime = {};
    logger->println(Logging::INFO, "Reading network time");
    if (!modem.getNetworkTime(&rawTime.tm_year, &rawTime.tm_mon, &rawTime.tm_mday, &rawTime.tm_hour, &rawTime.tm_min,
                              &rawTime.tm_sec, &timezone)) {
        logger->println(Logging::ERROR, "Time from network could not be obtained");
        return std::nullopt;
    }
    rawTime.tm_year -= 1900;
    rawTime.tm_mon -= 1;
    logger->printf(Logging::INFO, "Time is: %d:%d:%d\n", rawTime.tm_hour, rawTime.tm_min, rawTime.tm_sec);
    return {mktime(&rawTime)};
}

MODEM::STATUS_CODE GPS_TRACKER::SIM7000G::sleep() {
    bool res = modem.sleepEnable(true);
    pinMode(PIN_DTR, OUTPUT);
    digitalWrite(PIN_DTR, HIGH);
    delay(80);
    return res ? MODEM::STATUS_CODE::Ok : MODEM::STATUS_CODE::UNKNOWN_ERROR;
}

MODEM::STATUS_CODE GPS_TRACKER::SIM7000G::wakeUp() {
    logger->println(Logging::INFO, "Waking up SIM7000G");

    // POWER ON GSM MODULE
    pinMode(PWR_PIN, OUTPUT);
    digitalWrite(PWR_PIN, HIGH);
    delay(300);
    digitalWrite(PWR_PIN, LOW);

    pinMode(PIN_DTR, OUTPUT);
    digitalWrite(PIN_DTR, LOW);
    delay(80);
    bool res = modem.sleepEnable(false);
    return res ? MODEM::STATUS_CODE::Ok : MODEM::STATUS_CODE::UNKNOWN_ERROR;
}

MODEM::STATUS_CODE GPS_TRACKER::SIM7000G::sendActPosition() {
    if (!reconnectGSM()) {
        return UNKNOWN_ERROR;
    }
    GPSCoordinates coordinates;
    STATUS_CODE actPositionState = actualPosition(&coordinates);
    if (Ok != actPositionState) {
        logger->printf(Logging::WARNING, "Position is not valid, skipping: %d\n", actPositionState);
        return actPositionState;
    }
    stateManager->updatePosition(coordinates);
    Message message(configuration.CONFIG.trackerId, stateManager->getVisitedWaypoints(), coordinates,
                    batteryPercentage());
    return mqttClient.sendMessage(message) ? Ok : SENDING_DATA_FAILED;
}

MODEM::STATUS_CODE GPS_TRACKER::SIM7000G::sendPosition(GPSCoordinates position) {
    if (!reconnectGSM()) {
        return UNKNOWN_ERROR;
    }
    Message message(configuration.CONFIG.trackerId, stateManager->getVisitedWaypoints(), position,
                    batteryPercentage());
    return mqttClient.sendMessage(message) ? Ok : SENDING_DATA_FAILED;
}

void GPS_TRACKER::SIM7000G::powerOff() {
    modem.poweroff();
}


double GPS_TRACKER::SIM7000G::battery() {
    pinMode(PIN_ADC_BAT, INPUT);
    uint32_t in = 0;
    for (int i = 0; i < ADC_BATTERY_LEVEL_SAMPLES; i++) {
        in += (uint32_t) analogRead(PIN_ADC_BAT);
    }
    in = (int) in / ADC_BATTERY_LEVEL_SAMPLES;

    double bat_mv = ((float) in / 4096) * 3600 * 2;

    return bat_mv;
}

double GPS_TRACKER::SIM7000G::batteryPercentage() {
    return battery();
    double tmp = battery() - batteryDischargeVoltage;
    double per = (100 / (batteryFullyChargedLimit - batteryDischargeVoltage)) * tmp;
    return per > 0 ? per : 0;
}

STATUS_CODE SIM7000G::sendData(JsonDocument *data) {
    return mqttClient.sendData(data) ? Ok : SENDING_DATA_FAILED;
}

void SIM7000G::updateTime() {
    logger->println(Logging::DEBUG, "Setting time for logger");
    auto actTime = getActTime();
    if (!actTime.has_value()) {
        logger->println(Logging::ERROR, "Network time can not be obtained");
    } else {
        logger->printf(Logging::INFO, "Obtained time %l\n", actTime.value());
        auto *time = new timeval{static_cast<time_t>(actTime.value() - (3600 * 2)), 0};
        settimeofday(time, NULL);
    }
}

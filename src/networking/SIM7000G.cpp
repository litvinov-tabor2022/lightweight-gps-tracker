#include "SIM7000G.h"
#include "HwLocks.h"

MODEM::STATUS_CODE GPS_TRACKER::SIM7000G::init() {
    logger->println(Logging::INFO, "Initializing SIM700G module...");

    // POWER ON GSM MODULE
    logger->println(Logging::INFO, "Powering on SIM7000G");
    pinMode(PWR_PIN, OUTPUT);
    digitalWrite(PWR_PIN, HIGH);
    delay(300);
    digitalWrite(PWR_PIN, LOW);
    logger->println(Logging::INFO, "Waking up SIM7000G");

    if (configuration.GSM_CONFIG.enable) {
        logger->println(Logging::INFO, "Connecting to GSM/MQTT");
        if (!connectGPRS()) return GSM_CONNECTION_ERROR;
        mqttClient.init(configuration, logger, &gsmClientSSL);
        if (!mqttClient.begin()) return MQTT_CONNECTION_ERROR;
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
    std::lock_guard<std::recursive_mutex> lg(HwLocks::SERIAL_LOCK);
    if (!isConnected()) {
//        if (!reconnect()) {
//            logger->println(Logging::WARNING, "Modem is not connected");
//        }
        return MODEM_NOT_CONNECTED;
    }

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
        logger->printf(Logging::INFO, "lat: %f, lon: %f, alt: %f, timestamp: %ld\n", lat, lon, alt, timestamp);

        // Accuracy is below the minimal threshold
        if (accuracy > configuration.GPS_CONFIG.minimal_accuracy) {
            logger->printf(Logging::WARNING, "Accuracy is too low: %f < %f\n", accuracy,
                           configuration.GPS_CONFIG.minimal_accuracy);
            return GPS_ACCURACY_TOO_LOW;
        }

        *coordinates = GPSCoordinates(lat, lon, alt, timestamp);

        return Ok;
    }

    return READ_GPS_COORDINATES_FAILED;
}

bool GPS_TRACKER::SIM7000G::isConnected() {
    std::lock_guard<std::recursive_mutex> lg(HwLocks::SERIAL_LOCK);
    logger->printf(Logging::INFO, "is network connected: %b, is gprs connected %b\n", modem.isNetworkConnected(),
                   modem.isGprsConnected());
    return modem.isNetworkConnected() && modem.isGprsConnected();
}

bool GPS_TRACKER::SIM7000G::connectGPRS() {
    std::lock_guard<std::recursive_mutex> lg(HwLocks::SERIAL_LOCK);
    wakeUp();
    SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

    if (stateManager->getWakeupReason() != ESP_SLEEP_WAKEUP_TIMER) {
        if (!modem.restart()) {
            logger->println(Logging::WARNING, "Failed to restart modem.");
        }
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

    while (!modemConnectedSuccessfully) {
        logger->println(Logging::INFO, "Waiting for network...");
        if (!modem.waitForNetwork()) {
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
    return true;
}

bool GPS_TRACKER::SIM7000G::connectGPS() {
    std::lock_guard<std::recursive_mutex> lg(HwLocks::SERIAL_LOCK);
    wakeUp();

    modem.sendAT("+SGPIO=0,4,1,1");
    if (modem.waitResponse(10000L) != 1) {
        DBG(" SGPIO=0,4,1,1 false ");
    }

    if (!modem.enableGPS()) {
        logger->println(Logging::INFO, "Enabling GPS failed");
        return false;
    }

    // update file for fast fix once for 2.5 days
    logger->printf(Logging::INFO, "Time since last XTRA file update: %s\n",
                   stateManager->getLastFastFixFileUpdate() - getActTime());
    if (configuration.GPS_CONFIG.fastFix && stateManager->getLastFastFixFileUpdate() - getActTime() >= 216000) {
        fastFix();
        stateManager->setLastFastFixFileUpdate(getActTime());
        coldStart();
    } else {
        logger->println(Logging::INFO, "Skipping downloading of XTRA file");
        hotStart();
    }

    float lat, lon, speed, alt, accuracy;
    int vsat, usat;
    logger->println(Logging::INFO, "Waiting for GPS");
    while (!modem.getGPS(&lat, &lon, &speed, &alt, &vsat, &usat, &accuracy)) {
        if (this->isConnected()) {
            logger->println(Logging::DEBUG, modem.getGPSraw());
            logger->printf(Logging::DEBUG, "vsat: %d, usat: %d\n", vsat, usat);
            Tasker::sleep(1500);
        } else {
            return false;
        }
    }

    logger->println(Logging::INFO, "GPS successfully enabled");
    return true;
}

bool GPS_TRACKER::SIM7000G::isGpsConnected() {
    std::lock_guard<std::recursive_mutex> lg(HwLocks::SERIAL_LOCK);
    float lat, lon;
    return modem.getGPS(&lat, &lon);
}

bool GPS_TRACKER::SIM7000G::reconnect() {
    std::lock_guard<std::recursive_mutex> lg(HwLocks::SERIAL_LOCK);
    logger->println(Logging::INFO, "Reconnecting");
    int reconnectAttempts = 0;
    while (!isConnected() || !mqttClient.isConnected() || !isGpsConnected()) {
        wakeUp();
        if (reconnectAttempts > 5) {
            return false;
        }
        if (!isModemConnected()) {
            logger->println(Logging::INFO, "Reconnecting GPRS modem");
            if (!connectGPRS() ||
                !mqttClient.reconnect()) { // MQTT reconnect must be called even if the modem lost internet connection
                reconnectAttempts++;
                continue;
            }
        } else {
            logger->println(Logging::INFO, "GSM modem connected");
        }
        if (!isGpsConnected()) {
            logger->println(Logging::INFO, "Reconnecting GPS");
            if (!connectGPS()) {
                reconnectAttempts++;
                continue;
            }
        } else {
            Serial.println("GPS modem connected");
        }
        if (!mqttClient.isConnected()) {
            logger->println(Logging::INFO, "Reconnecting MQTT client");
            if (!mqttClient.reconnect()) {
                reconnectAttempts++;
                continue;
            }
        } else {
            logger->println(Logging::INFO, "MQTT modem connected");
        }
    }
    logger->println(Logging::INFO, "Reconnecting done");
    return true;
}

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

GPS_TRACKER::Timestamp GPS_TRACKER::SIM7000G::getActTime() {
    float timezone;
    tm rawTime = {};
    modem.getNetworkTime(&rawTime.tm_year, &rawTime.tm_mon, &rawTime.tm_mday, &rawTime.tm_hour, &rawTime.tm_min,
                         &rawTime.tm_sec, &timezone);
    rawTime.tm_year -= 1900;
    rawTime.tm_mon -= 1;
    return mktime(&rawTime);
}

MODEM::STATUS_CODE GPS_TRACKER::SIM7000G::sleep() {
    bool res = modem.sleepEnable(true);
    pinMode(PIN_DTR, OUTPUT);
    digitalWrite(PIN_DTR, HIGH);
    delay(80);
    return res ? MODEM::STATUS_CODE::Ok : MODEM::STATUS_CODE::UNKNOWN_ERROR;
}

MODEM::STATUS_CODE GPS_TRACKER::SIM7000G::wakeUp() {
    pinMode(PIN_DTR, OUTPUT);
    digitalWrite(PIN_DTR, LOW);
    delay(80);
    bool res = modem.sleepEnable(false);
    return res ? MODEM::STATUS_CODE::Ok : MODEM::STATUS_CODE::UNKNOWN_ERROR;
}

MODEM::STATUS_CODE GPS_TRACKER::SIM7000G::sendActPosition() {
    if (!reconnect()) {
        return SENDING_DATA_FAILED;
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

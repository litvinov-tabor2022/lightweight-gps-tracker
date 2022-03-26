#include "SIM7000G.h"

MODEM::STATUS_CODE GPS_TRACKER::SIM7000G::init() {

    // POWER ON GSM MODULE
    pinMode(PWR_PIN, OUTPUT);
    digitalWrite(PWR_PIN, HIGH);
    delay(300);
    digitalWrite(PWR_PIN, LOW);

    if (configuration.GSM_CONFIG.enable) {
        if (!connectGPRS()) return GSM_CONNECTION_ERROR;
        connectToMqtt();
    }

    if (configuration.GPS_CONFIG.enable) {
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

    if (configuration.GSM_CONFIG.enable) {
        DefaultTasker.loopEvery("mqtt", 100, [this] {
            std::lock_guard<std::recursive_mutex> lg(gsm_mutex);
            mqttClient.loop();
        });
        logger->println(Logging::INFO, "Modem connected to MQTT");
    }
    return Ok;
}

MODEM::STATUS_CODE GPS_TRACKER::SIM7000G::sendData(const std::string &data) {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);

    logger->printf(Logging::INFO, "Sending data... %d\n", mqttClient.state());
    if (!mqttClient.connected()) {
        logger->printf(Logging::ERROR, "MQTT client error: %d\n", mqttClient.state());
        logger->println(Logging::INFO, "Trying to reconnect ...");
        if (!reconnect()) return UNKNOWN_ERROR;
    }

    bool published = mqttClient.publish(configuration.MQTT_CONFIG.topic.c_str(), data.c_str());
    logger->printf(Logging::INFO, "Publish %d chars to MQTT topic %s end with result: %d\n", data.length(),
                   configuration.MQTT_CONFIG.topic.c_str(), published);

    if (published) {
        return Ok;
    }

    return READ_GPS_COORDINATES_FAILED;
}

MODEM::STATUS_CODE GPS_TRACKER::SIM7000G::sendData(JsonDocument *data) {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);

    std::string serialized;
    serializeJson(*data, serialized);

    return sendData(serialized);
}

MODEM::STATUS_CODE GPS_TRACKER::SIM7000G::actualPosition(GPSCoordinates *coordinates) {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);
    if (!isConnected()) {
        reconnect();
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

//    reconnect();

    return READ_GPS_COORDINATES_FAILED;
}

bool GPS_TRACKER::SIM7000G::isConnected() {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);
    return modem.isNetworkConnected() && modem.isGprsConnected();
}

bool GPS_TRACKER::SIM7000G::connectGPRS() {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);

//    TinyGsmAutoBaud(SerialAT, 9600, 115200);
    SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

    // TODO: is this needed
    if (!modem.restart()) {
        logger->println(Logging::ERROR, "Failed to restart modem, attempting to continue without restarting");
        return false;
    }

    modem.sleepEnable(false);

    logger->printf(Logging::INFO, "Modem: %s\n", modem.getModemInfo().c_str());

    /*
      2 Automatic
      13 GSM only
      38 LTE only
      51 GSM and LTE only
    * * * */
    String res;
    res = modem.setNetworkMode(2);
    if (res != "1") {
        logger->println(Logging::ERROR, "setNetworkMode  false ");
        return false;
    }

    /*
      1 CAT-M
      2 NB-Iot
      3 CAT-M and NB-IoT
    * * */
    res = modem.setPreferredMode(2);
    if (res != "1") {
        logger->println(Logging::ERROR, "setPreferredMode  false ");
        return false;
    }

    bool modemConnectedSuccessfully = false;

    while (!modemConnectedSuccessfully) {
//        Tasker::sleep(1000);
        logger->println(Logging::INFO, "Waiting for network...");
        if (!modem.waitForNetwork()) {
            logger->println(Logging::ERROR, " failed!");
            continue;
        }

        logger->printf(Logging::INFO, "Connecting to %s", configuration.GSM_CONFIG.apn.c_str());
        if (!modem.gprsConnect(configuration.GSM_CONFIG.apn.c_str(),
                               configuration.GSM_CONFIG.user.c_str(),
                               configuration.GSM_CONFIG.password.c_str())) {
            logger->println(Logging::ERROR, " failed!");
            continue;
        }
        logger->println(Logging::INFO, " succeed!");
        modemConnectedSuccessfully = true;
    }
//    stateManager->setGsmState(GSM::CONNECTED);
    return true;
}

bool GPS_TRACKER::SIM7000G::connectToMqtt() {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);

    logger->println(Logging::INFO, "Connecting to MQTT....");
    mqttClient.setServer(configuration.MQTT_CONFIG.host.c_str(), configuration.MQTT_CONFIG.port);

    //    mqttClient.setKeepAlive((REPORT_FREQUENCE * 2) / 1000);
    int errorAttempts = 0;

    // Loop until we're reconnected
    while (!mqttClient.connected()) {
        Tasker::yield();
        // Create a random client ID
        String clientId = "TRACKER-";
        clientId += String(random(0xffff), HEX);
        // Attempt to connect
        logger->printf(Logging::INFO, "Attempting MQTT connection... host: %s, username: %s, password: %s\n",
                       configuration.MQTT_CONFIG.host.c_str(),
                       configuration.MQTT_CONFIG.username.c_str(), configuration.MQTT_CONFIG.password.c_str());
        mqttClient.disconnect();
        if (mqttClient.connect(clientId.c_str(),
                               configuration.MQTT_CONFIG.username.c_str(),
                               configuration.MQTT_CONFIG.password.c_str())) {
//            stateManager->setMqttState(MQTT::CONNECTED);
            logger->printf(Logging::INFO, " connected to %s, topic: %s, username: %s, password: %s\n",
                           configuration.MQTT_CONFIG.host.c_str(), configuration.MQTT_CONFIG.topic.c_str(),
                           configuration.MQTT_CONFIG.username.c_str(), configuration.MQTT_CONFIG.password.c_str());
        } else {
//            stateManager->setMqttState(MQTT::DISCONNECTED);
            logger->printf(Logging::ERROR, "failed, rc=%d try again in 5 seconds\n", mqttClient.state());
            errorAttempts++;
            if (errorAttempts > 5) logger->println(Logging::ERROR, "MQTT connection error.");
            // Wait 5 seconds before retrying
            Tasker::sleep(5000);
        }
    }

    return true;
}

bool GPS_TRACKER::SIM7000G::connectGPS() {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);

    // TODO: mozna to bude potreba
    modem.sendAT("+SGPIO=0,4,1,1");
    if (modem.waitResponse(10000L) != 1) {
        DBG(" SGPIO=0,4,1,1 false ");
    }

    if (configuration.GPS_CONFIG.fastFix) {
        fastFix();
    }

    if (!modem.enableGPS()) {
        logger->println(Logging::INFO, "Enabling GPS failed");
    }

    float lat, lon, speed, alt, accuracy;
    int vsat, usat;
    logger->println(Logging::INFO, "Waiting for GPS");
    while (!modem.getGPS(&lat, &lon, &speed, &alt, &vsat, &usat, &accuracy)) {
        if (this->isConnected()) {
            logger->println(Logging::INFO, modem.getGPSraw());
            logger->printf(Logging::INFO, "vsat: %d, usat: %d\n", vsat, usat);
            Tasker::sleep(1500);
        } else {
            return false;
        }
    }

    logger->println(Logging::INFO, "GPS successfully enabled");
    return true;
}

bool GPS_TRACKER::SIM7000G::isGpsConnected() {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);
    float lat, lon;
    return modem.getGPS(&lat, &lon);
}

bool GPS_TRACKER::SIM7000G::isMqttConnected() {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);
    if (mqttClient.connected()) {
        logger->println(Logging::INFO, "MQTT client connected");
        return true;
    } else {
        logger->println(Logging::INFO, "MQTT client disconnected");
        return false;
    }
}


bool GPS_TRACKER::SIM7000G::reconnect() {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);
    logger->println(Logging::INFO, "Reconnecting");
    int reconnectAttempts = 0;
    while (!isConnected() || !isMqttConnected() || !isGpsConnected()) {
        if (reconnectAttempts > 5) {
//            esp_restart(); // TODO skipp if playing music
//            return false;
        }
        if (!isModemConnected()) {
            logger->println(Logging::INFO, "Reconnecting GPRS modem");
            if (!connectGPRS()) {
                reconnectAttempts++;
            }
        } else {
            logger->println(Logging::INFO, "GSM modem connected");
        }
        if (!isGpsConnected()) {
            logger->println(Logging::INFO, "Reconnecting GPS");
            if (!connectGPS()) {
                reconnectAttempts++;
            }
        } else {
            Serial.println("GPS modem connected");
        }
        if (!isMqttConnected()) {
            logger->println(Logging::INFO, "Reconnecting MQTT client");
            if (!connectToMqtt()) {
                reconnectAttempts++;
            }
        } else {
            logger->println(Logging::INFO, "MQTT modem connected");
        }
    }
    logger->println(Logging::INFO, "Reconnecting done");
    return true;
}

// FIXME: hardcoded APN
void GPS_TRACKER::SIM7000G::fastFix() {
    modem.sendAT(GF("+CGNSMOD=1,1,1,1"));
    modem.waitResponse();
    modem.sendAT(GF("+SAPBR=3,1, \"APN\",\"internet.t-mobile.cz\""));
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
    modem.sendAT(GF("+CGNSCOLD"));
    modem.waitResponse(5000L, GF("+CGNSXTRA: 0"));
}

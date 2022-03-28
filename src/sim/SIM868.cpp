#include "SIM868.h"

GPS_TRACKER::SIM868::SIM868() {}

MODEM::STATUS_CODE GPS_TRACKER::SIM868::init() {
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
        else Serial.println("Modem connected to GPS");
    }

    if (configuration.GSM_CONFIG.enable) {
        DefaultTasker.loopEvery("mqtt", 100, [this] {
            std::lock_guard<std::recursive_mutex> lg(gsm_mutex);
            mqttClient.loop();
        });
        Serial.println("Modem connected to MQTT");
    }
    return Ok;
}

MODEM::STATUS_CODE GPS_TRACKER::SIM868::sendData(const std::string &data) {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);

    Serial.printf("Sending data\n");
    if (!mqttClient.connected()) {
        Serial.printf("MQTT client error: %d\n", mqttClient.state());
        Serial.println("Trying to reconnect ...");
        if (!reconnect()) return UNKNOWN_ERROR;
    }

    bool published = mqttClient.publish(configuration.MQTT_CONFIG.topic.c_str(), data.c_str());
    Serial.printf("Publish %d chars to MQTT topic %s end with result [1 - success, 0 - fail]: %d\n", data.length(),
                  configuration.MQTT_CONFIG.topic.c_str(), published);

    if (published) {
        return Ok;
    }

    return READ_GPS_COORDINATES_FAILED;
}

MODEM::STATUS_CODE GPS_TRACKER::SIM868::sendData(JsonDocument *data) {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);

    std::string serialized;
    serializeJson(*data, serialized);

    return sendData(serialized);
}

MODEM::STATUS_CODE GPS_TRACKER::SIM868::actualPosition(GPSCoordinates *coordinates) {
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
            Serial.println("Invalid position read");
            return GPS_COORDINATES_OUT_OF_RANGE;
        }

        rawTime.tm_year -= 1900;
        rawTime.tm_mon -= 1;
        long timestamp = mktime(&rawTime);
        Serial.printf("lat: %f, lon: %f, alt: %f, timestamp: %ld\n", lat, lon, alt, timestamp);

        // Accuracy is below the minimal threshold
        if (accuracy > configuration.GPS_CONFIG.minimal_accuracy) {
            Serial.printf("Accuracy is too low: %f < %f\n", accuracy, configuration.GPS_CONFIG.minimal_accuracy);
            return GPS_ACCURACY_TOO_LOW;
        }

        *coordinates = GPSCoordinates(lat, lon, alt, timestamp);

        return Ok;
    }

    reconnect();

    return READ_GPS_COORDINATES_FAILED;
}

bool GPS_TRACKER::SIM868::isConnected() {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);
    return modem.isNetworkConnected() && modem.isGprsConnected();
}

bool GPS_TRACKER::SIM868::connectGPRS() {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);

    TinyGsmAutoBaud(Serial2, 9600, 115200);

    if (!modem.init()) {
        Serial.println("Modem connection failed.");
        return false;
    }

    modem.sleepEnable(false);

    Serial.printf("[I] Modem: %s\n", modem.getModemInfo().c_str());

    bool modemConnectedSuccessfully = false;

    while (!modemConnectedSuccessfully) {
//        Tasker::sleep(1000);
        Serial.println("Waiting for network...");
        if (!modem.waitForNetwork()) {
            Serial.println(" failed!");
            continue;
        }

        Serial.printf("Connecting to %s", configuration.GSM_CONFIG.apn.c_str());
        if (!modem.gprsConnect(configuration.GSM_CONFIG.apn.c_str(),
                               configuration.GSM_CONFIG.user.c_str(),
                               configuration.GSM_CONFIG.password.c_str())) {
            Serial.println(" failed!");
            continue;
        }
        Serial.println(" succeed!");
        modemConnectedSuccessfully = true;
    }
//    stateManager->setGsmState(GSM::CONNECTED);
    return true;
}

bool GPS_TRACKER::SIM868::connectToMqtt() {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);

    Serial.println("Connecting to MQTT....");
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
        Serial.printf("Attempting MQTT connection... host: %s, username: %s, password: %s\n",
                      configuration.MQTT_CONFIG.host.c_str(),
                      configuration.MQTT_CONFIG.username.c_str(), configuration.MQTT_CONFIG.password.c_str());
        mqttClient.disconnect();
        if (mqttClient.connect(clientId.c_str(),
                               configuration.MQTT_CONFIG.username.c_str(),
                               configuration.MQTT_CONFIG.password.c_str())) {
//            stateManager->setMqttState(MQTT::CONNECTED);
            Serial.printf(" connected to %s, topic: %s, username: %s, password: %s\n",
                          configuration.MQTT_CONFIG.host.c_str(), configuration.MQTT_CONFIG.topic.c_str(),
                          configuration.MQTT_CONFIG.username.c_str(), configuration.MQTT_CONFIG.password.c_str());
        } else {
//            stateManager->setMqttState(MQTT::DISCONNECTED);
            Serial.printf("failed, rc=%d try again in 5 seconds\n", mqttClient.state());
            errorAttempts++;
            if (errorAttempts > 5) Serial.println("MQTT connection error.");
            // Wait 5 seconds before retrying
            Tasker::sleep(5000);
        }
    }

    return true;
}

bool GPS_TRACKER::SIM868::connectGPS() {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);

    modem.disableGPS();

    if (configuration.GPS_CONFIG.fastFix) {
        fastFix();
    }

    if (!modem.enableGPS()) {
        Serial.println("Enabling GPS failed");
    }

    float lat, lon, speed, alt, accuracy;
    int vsat, usat;
    Serial.println("Waiting for GPS");
    while (!modem.getGPS(&lat, &lon, &speed, &alt, &vsat, &usat, &accuracy)) {
        if (this->isConnected()) {
            Serial.println(modem.getGPSraw());
            Serial.printf("vsat: %d, usat: %d\n", vsat, usat);
            Tasker::sleep(1500);
        } else {
            return false;
        }
    }

    Serial.println("GPS successfully enabled");
    return true;
}

bool GPS_TRACKER::SIM868::isGpsConnected() {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);
    float lat, lon;
    return modem.getGPS(&lat, &lon);
}

bool GPS_TRACKER::SIM868::isMqttConnected() {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);
    if (mqttClient.connected()) {
        Serial.println("MQTT client connected");
        return true;
    } else {
        Serial.println("MQTT client disconnected");
        return false;
    }
}


bool GPS_TRACKER::SIM868::reconnect() {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);
    Serial.println("Reconnecting");
    int reconnectAttempts = 0;
    while (!isConnected() || !isMqttConnected() || !isGpsConnected()) {
        if (reconnectAttempts > 5) {
            esp_restart(); // TODO skipp if playing music
//            return false;
        }
        if (!isModemConnected()) {
            Serial.println("Reconnecting GPRS modem");
            if (!connectGPRS()) {
                reconnectAttempts++;
            }
        } else {
            Serial.println("GSM modem connected");
        }
        if (!isGpsConnected()) {
            Serial.println("Reconnecting GPS");
            if (!connectGPS()) {
                reconnectAttempts++;
            }
        } else {
            Serial.println("GPS modem connected");
        }
        if (!isMqttConnected()) {
            Serial.println("Reconnecting MQTT client");
            if (!connectToMqtt()) {
                reconnectAttempts++;
            }
        } else {
            Serial.println("MQTT modem connected");
        }
    }
    Serial.println("Reconnecting done");
    return true;
}

void GPS_TRACKER::SIM868::fastFix() {
    Serial.println("FastFix");

    bool responseStatus = false;
    modem.sendAT(GF("+SAPBR=3,1,\"CONTYPE\",\"GPRS\""));
    modem.waitResponse();
    delay(20);

    modem.sendAT(GF("+SAPBR=3,1,\"APN\",\"internet.t-mobile.cz\""));
    modem.waitResponse();
    delay(20);

    modem.sendAT(GF("+SAPBR=1,1"));
    modem.waitResponse();
    delay(20);

    modem.sendAT(GF("+SAPBR=2,1"));
    modem.waitResponse();
    delay(20);

    modem.sendAT(GF("+CNTPCID=1"));
    modem.waitResponse();
    delay(20);

    modem.sendAT(GF("+CNTP=\"202.112.29.82\""));
    modem.waitResponse();

//    modem.sendAT(GF("+CNTP?"));
//    modem.waitResponse();
    delay(20);

    modem.sendAT(GF("+CNTP"));
    modem.waitResponse(1000L, GF("+CNTP: 1"));

//    modem.sendAT(GF("+CCLK?"));
//    modem.waitResponse();
    delay(20);

    modem.sendAT(GF("+CLBS=1,1"));
    modem.waitResponse();
    delay(20);

    modem.sendAT(GF("+CGNSSAV=3,3"));
    modem.waitResponse();
    delay(20);

    // HTTP REQUEST
    modem.sendAT(GF("+HTTPINIT"));
    modem.waitResponse();
    delay(20);

    modem.sendAT(GF("+HTTPPARA=\"CID\",1"));
    modem.waitResponse();
    delay(20);

    modem.sendAT(GF("+HTTPPARA=\"URL\",\"http://wepodownload.mediatek.com/EPO_GPS_3_1.DAT\""));
    modem.waitResponse();
    delay(20);

    modem.sendAT(GF("+HTTPACTION=0"));
    modem.waitResponse(20000L, GF("+HTTPACTION: 0,200,27648"));
    delay(20);

    modem.sendAT(GF("+HTTPTERM"));
    modem.waitResponse();
    delay(20);

//    modem.sendAT(GF("+FSLS=C:\\User\\"));
//    modem.waitResponse();
//    modem.waitResponse();
//    modem.waitResponse();

    modem.sendAT(GF("+CGNSCHK=3,1"));
    modem.waitResponse();
    delay(20);

//    modem.sendAT(GF("+CGNSPWR=1"));
//    if(modem.waitResponse(10000L, GF("+CGNSPWR: 1"))){
//        Serial.println("FastFix: GPS power on failed");
//    }

    modem.sendAT(GF("+CGNSAID=31,1,1"));
    if (modem.waitResponse(20000L, GF("+CGNSAID")) != 1) {
        Serial.println("FastFix: Upload failed");
    }

    Serial.println("FastFix: DONE");
}

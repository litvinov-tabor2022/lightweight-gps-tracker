#include "SIM868.h"

ISIM::STATUS_CODE GPS_TRACKER::SIM868::init() {
    TinyGsmAutoBaud(Serial2, 9600, 115200);
//    modem.simUnlock("1234");
    if (!modem.init()) {
        Serial.println("Modem connection failed.");
        return MODEM_INIT_FAILED;
    }
    modem.sleepEnable(false);

    if (configuration.GSM_CONFIG.enable) {
        if (!connectGPRS()) return GSM_CONNECTION_ERROR;
        connectToMqtt();
        DefaultTasker.loopEvery("mqtt", 100, [this] {
            std::lock_guard<std::recursive_mutex> lg(gsm_mutex);
            mqttClient.loop();
        });
        Serial.println("Modem connected to MQTT");
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


    return Ok;
}

ISIM::STATUS_CODE GPS_TRACKER::SIM868::sendData(const std::string &data) {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);

    if (mqttClient.state() != 0) {
        Serial.printf("MQTT client error: %d\n", mqttClient.state());
        return UNKNOWN_ERROR;
    }

    bool published = mqttClient.publish(configuration.MQTT_CONFIG.topic.c_str(), data.c_str());
    Serial.printf("Publish %d chars to MQTT end with result: %d\n", data.length(), published);

    if (published) {
        return Ok;
    }

    return READ_GPS_COORDINATES_FAILED;
    return ISIM::Ok;
}

ISIM::STATUS_CODE GPS_TRACKER::SIM868::sendData(JsonDocument *data) {

//    GPS_COORDINATES actCoordinates;
//    ISIM::STATUS_CODE actPositionStatus = actualPosition(actCoordinates);
//    if (actPositionStatus != Ok) {
//        return actPositionStatus;
//    }

//    uint8_t buffer[1024];
//    size_t message_length;

//    actCoordinates.serialize(buffer, message_length);

    std::string serialized;
    serializeJson(*data, serialized);

    return sendData(serialized);
}

ISIM::STATUS_CODE GPS_TRACKER::SIM868::actualPosition(GPS_COORDINATES &coordinates) {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);
    if (!isConnected()) {
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
            return UNKNOWN_ERROR;
        }

        // Accuracy is below the minimal threshold
        if (accuracy > configuration.GPS_CONFIG.minimal_accuracy) {
            Serial.printf("Accuracy is to low: %f\n", accuracy);
            return UNKNOWN_ERROR;
        }

        rawTime.tm_year -= 1900;
        rawTime.tm_mon -= 1;
        long timestamp = mktime(&rawTime);

        coordinates = GPS_COORDINATES{lat, lon, alt, timestamp};

        return Ok;
    }

    return READ_GPS_COORDINATES_FAILED;
}

bool GPS_TRACKER::SIM868::isConnected() {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);
    return modem.isNetworkConnected() && modem.isGprsConnected();
}

bool GPS_TRACKER::SIM868::connectGPRS() {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);
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
    return true;
}

bool GPS_TRACKER::SIM868::connectToMqtt() {
    mqttClient.setServer(configuration.MQTT_CONFIG.host.c_str(), configuration.MQTT_CONFIG.port);
//    mqttClient.setKeepAlive((REPORT_FREQUENCE * 2) / 1000);

    int errorAttempts = 0;
    // Loop until we're reconnected
    while (!mqttClient.connected()) {
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
            Serial.println(sendData("Hello"));
            Serial.printf(" connected to %s, topic: %s, username: %s, password: %s\n",
                          configuration.MQTT_CONFIG.host.c_str(), configuration.MQTT_CONFIG.topic.c_str(),
                          configuration.MQTT_CONFIG.username.c_str(), configuration.MQTT_CONFIG.password.c_str());
        } else {
            Serial.printf("failed, rc=%d try again in 5 seconds\n", mqttClient.state());
            errorAttempts++;
            if (errorAttempts > 5) Serial.println("MQTT connection error.");
            // Wait 5 seconds before retrying
//            Tasker::sleep(5000);
        }
    }

    return true;
}

bool GPS_TRACKER::SIM868::connectGPS() {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);

    if (!modem.enableGPS()) {
        return false;
    }

    float lat, lon, speed, alt, accuracy;
    int vsat, usat;
    Serial.print("Waiting for GPS");
    while (!modem.getGPS(&lat, &lon, &speed, &alt, &vsat, &usat, &accuracy)) {
        if (this->isConnected()) {
            Tasker::sleep(1500);
        } else {
            return false;
        }
    }
    Serial.println();

    Serial.println("GPS successfully enabled");
    return true;
}

bool GPS_TRACKER::SIM868::reconnect() {
    std::lock_guard<std::recursive_mutex> lg(gsm_mutex);
    while (!isConnected()) {
        if (!connectGPRS()) {
            continue;
        }
        if (!connectGPS()) {
            continue;
        }
    }

    return true;
}

GPS_TRACKER::SIM868::SIM868() {}

namespace GPS_TRACKER {
}

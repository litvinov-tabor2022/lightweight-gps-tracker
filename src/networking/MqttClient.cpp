#include "MqttClient.h"
#include "HwLocks.h"

void MqttClient::init(Configuration &config, Logging::Logger *log, Client *client) {
    this->logger = log;
    this->configuration = config;
    this->net = client;
}

bool MqttClient::begin() {
    if (connect()) {
        DefaultTasker.loopEvery("mqtt", 250, [this] {
            std::lock_guard<std::recursive_mutex> lg(HwLocks::SERIAL_LOCK);
            if (!mqttClient.loop()) {
                logger->printf(Logging::WARNING, "MQTT loop error: %d\n", mqttClient.lastError());
            }
        });
        return true;
    }
    return false;
}

bool MqttClient::connect() {
    logger->println(Logging::INFO, "Connecting to MQTT....");
    mqttClient.begin(configuration.MQTT_CONFIG.host.c_str(), configuration.MQTT_CONFIG.port, *net);
    mqttClient.setKeepAlive(configuration.CONFIG.sleepTime * uS_TO_S_FACTOR * 2);

    if (reconnect()) {
        logger->println(Logging::INFO, "Modem connected to MQTT");
        return true;
    } else {
        logger->println(Logging::ERROR, "Connecting to MQTT failed");
        return false;
    }
}

bool MqttClient::reconnect(int maxAttempts) {
    std::lock_guard<std::recursive_mutex> lg(HwLocks::SERIAL_LOCK);

    mqttClient.setHost(configuration.MQTT_CONFIG.host.c_str(), configuration.MQTT_CONFIG.port);

    int errorAttempts = 0;
    while (!mqttClient.connected()) {
        String clientId = "TRACKER-" + (String) configuration.CONFIG.trackerId;
        // Attempt to connect
        logger->printf(Logging::INFO, "Attempting MQTT connection... host: %s, username: %s, password: %s\n",
                       configuration.MQTT_CONFIG.host.c_str(),
                       configuration.MQTT_CONFIG.username.c_str(), configuration.MQTT_CONFIG.password.c_str());
        String willMessage = (String) configuration.CONFIG.trackerId + " is offline";
        if (mqttClient.connect(clientId.c_str(),
                               configuration.MQTT_CONFIG.username.c_str(),
                               configuration.MQTT_CONFIG.password.c_str())) {
            logger->printf(Logging::INFO, " connected to %s, topic: %s, username: %s, password: %s\n",
                           configuration.MQTT_CONFIG.host.c_str(), configuration.MQTT_CONFIG.topic.c_str(),
                           configuration.MQTT_CONFIG.username.c_str(), configuration.MQTT_CONFIG.password.c_str());
        } else {
            // connection failed
            logger->printf(Logging::ERROR, "failed, try again in 5 seconds, attempt no. %d\n",
                           errorAttempts);
            errorAttempts++;
            if (errorAttempts > maxAttempts) {
                logger->println(Logging::ERROR, "MQTT connection error.");
                return false;
            }
            // Wait 5 seconds before retrying
            Tasker::sleep(5000);
        }
    }

    return true;
}

bool MqttClient::isConnected() {
    std::lock_guard<std::recursive_mutex> lg(HwLocks::SERIAL_LOCK);
    return mqttClient.connected();
}

bool MqttClient::sendString(const std::string &data) {
    std::lock_guard<std::recursive_mutex> lg(HwLocks::SERIAL_LOCK);

    logger->println(Logging::INFO, "Start sending routine...");
    if (!isConnected()) {
        logger->println(Logging::ERROR, "MQTT client error while sending data: %d\n");
        return false;
    }

    bool published = mqttClient.publish(configuration.MQTT_CONFIG.topic.c_str(), data.c_str(), false, 1);
    logger->printf(Logging::INFO, "Publish %d chars to MQTT topic %s ends with result: %d\n", data.length(),
                   configuration.MQTT_CONFIG.topic.c_str(), published);

    return published;
}

bool MqttClient::sendMessage(const Message &message) {
    std::string serializeMessage;
    if (!message.serialize(serializeMessage)) {
        logger->println(Logging::ERROR, "Message serialization error");
        return false;
    }

    logger->printf(Logging::INFO, "Message: %s\n", serializeMessage.c_str());

    return sendString(serializeMessage);
}

bool MqttClient::sendData(JsonDocument *data) {
    std::lock_guard<std::recursive_mutex> lg(HwLocks::SERIAL_LOCK);

    std::string serialized;
    serializeJson(*data, serialized);

    return sendString(serialized);
}

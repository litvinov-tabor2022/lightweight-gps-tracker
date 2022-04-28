#ifndef LIGHTWEIGHT_GPS_TRACKER_MQTTCLIENT_H
#define LIGHTWEIGHT_GPS_TRACKER_MQTTCLIENT_H

#include <MQTT.h>
#include <mutex>
#include "Configuration.h"
#include "Tasker.h"
#include "logger/Logger.h"
#include "Protocol.h"

using namespace GPS_TRACKER;

class MqttClient {
public:
    MqttClient() {};

    void init(Configuration &config, Logging::Logger *logger, Client *client);

    bool begin();

    /**
     * Function tries to connect to MQTT broker. Attempts are passed as parameter.
     * Delays between attempts are fixed to 5 seconds.
     * */
    bool reconnect(int maxAttempts = 5);

    bool isConnected();

    bool sendString(const std::string &data);

    bool sendMessage(const Message &message);

    bool sendData(JsonDocument *data);

private:
    /**
     * Connects module to MQTT broker. Set necessary client information, initialize `mqttClient`.
     *
     * Internally calls `reconnect()`.
     *
     * @return true - if connection succeed
     * @return false - otherwise
     * */
    bool connect();

    MQTTClient mqttClient = MQTTClient(1024);
    GPS_TRACKER::Configuration configuration;
    Client *net;
    Logging::Logger *logger;
};


#endif //LIGHTWEIGHT_GPS_TRACKER_MQTTCLIENT_H

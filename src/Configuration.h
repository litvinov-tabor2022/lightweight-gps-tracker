#ifndef LIGHTWEIGHT_GPS_TRACKER_CONFIGURATION_H
#define LIGHTWEIGHT_GPS_TRACKER_CONFIGURATION_H

#include <utility>
#include <map>
#include <queue>

#include "SPIFFS.h"
#include "ArduinoJson.h"
#include "Constants.h"
#include "string"

namespace GPS_TRACKER {
    struct gps_config {
        explicit gps_config() = default;

        gps_config(bool enable, bool fastFix, double minimalAccuracy) :
                enable(enable),
                fastFix(fastFix),
                minimal_accuracy(minimalAccuracy) {}

        static gps_config build(JsonVariant &c) {
            return {
                    c["enable"].as<bool>(),
                    c["fast-fix"].as<bool>(),
                    c["minimal-accuracy"].as<double>()
            };
        }

        bool enable = false;
        bool fastFix = false;
        double minimal_accuracy = 0;
    };

    struct gsm_config {
        explicit gsm_config() = default;

        gsm_config(bool enable, std::string apn, std::string user, std::string password, int reconnectTime) : enable(
                enable), apn(std::move(apn)), user(std::move(user)), password(std::move(password)), reconnectTime(
                reconnectTime) {}

        static gsm_config build(JsonVariant &c) {
            return {
                    c["enable"].as<bool>(),
                    c["apn"].as<std::string>(),
                    c["user"].as<std::string>(),
                    c["password"].as<std::string>(),
                    c["reconnect-time"].as<int>()
            };
        }

        bool enable = false;
        std::string apn;
        std::string user;
        std::string password;
        int reconnectTime = 0;
    };

    struct mqtt_config {
        mqtt_config() = default;

        mqtt_config(std::string topic, std::string host, std::string username,
                    std::string password, int port) : topic(std::move(topic)), host(std::move(host)),
                                                      username(std::move(username)),
                                                      password(std::move(password)), port(port) {}

        static mqtt_config build(JsonVariant &c) {
            return mqtt_config(
                    c["topic"].as<std::string>(),
                    c["host"].as<std::string>(),
                    c["username"].as<std::string>(),
                    c["password"].as<std::string>(),
                    c["port"].as<int>()
            );
        }

        std::string topic;
        std::string host;
        std::string username;
        std::string password;
        int port = 8883;
    };

    struct config {
        config() = default;

        config(long trackerId, std::string token, double accuracy, long sleepTime) : trackerId(trackerId),
                                                                                     accuracy(accuracy),
                                                                                     token(std::move(token)),
                                                                                     sleepTime(sleepTime) {}

        static config build(JsonVariant &c) {
            return config(
                    c["tracker-id"].as<long>(),
                    c["token"].as<std::string>(),
                    c["accuracy"].as<double>(),
                    c["sleep-time"].as<long>()
            );
        }

        long trackerId = -1;
        double accuracy = 100;
        std::string token;
        long sleepTime = 0; // in seconds
    };

    struct waypoint {
        waypoint(size_t id, float lat, float lon, std::string path) : id(id), lat(lat), lon(lon),
                                                                      path(std::move(path)) {}

        size_t id;
        float lat;
        float lon;
        std::string path;
    };

    using waypoints = std::vector<waypoint>;

    class Configuration {
    public:
        /**
         * This functions is accessing configuration file on external storage.
         * */
        bool read() {
            DynamicJsonDocument doc(2048);

            // read config from filesystem
            File configFile = SPIFFS.open(CONFIG_PATH.c_str());
            DeserializationError error = deserializeJson(doc, configFile);
            configFile.close();

            if (error != DeserializationError::Ok) {
                return false;
            }

            // parsing
            JsonVariant generalConfig = doc["general"];
            JsonVariant mqttConfiguration = doc["mqtt"];
            JsonVariant gsmConfig = doc["gsm"];
            JsonVariant gpsConfig = doc["gps"];
            JsonArray waypoints = doc["waypoints"].as<JsonArray>();

            GPS_CONFIG = gps_config::build(gpsConfig);
            GSM_CONFIG = gsm_config::build(gsmConfig);
            MQTT_CONFIG = mqtt_config::build(mqttConfiguration);
            CONFIG = config::build(generalConfig);

            for (JsonVariant v: waypoints) {
                size_t id = v["id"];
                std::string path = v["path"];
                float lat = v["lat"];
                float lon = v["lon"];
                WAYPOINTS.push_back(waypoint(id, lat, lon, path));
            }

            return true;
        }

        gps_config GPS_CONFIG;
        gsm_config GSM_CONFIG;
        mqtt_config MQTT_CONFIG;
        config CONFIG;
        waypoints WAYPOINTS;
    };
}

#endif //LIGHTWEIGHT_GPS_TRACKER_CONFIGURATION_H

#ifndef LIGHTWEIGHT_GPS_TRACKER_PROTOCOL_H
#define LIGHTWEIGHT_GPS_TRACKER_PROTOCOL_H

#include <utility>
#include "Arduino.h"

namespace GPS_TRACKER {
    typedef unsigned long Timestamp;

    struct Serializable {
        virtual bool serialize(std::string &buffer) const = 0;

        [[nodiscard]] virtual JsonVariant toJson() const = 0;
    };

    struct GPSCoordinates : Serializable {
        GPSCoordinates() {};

        GPSCoordinates(float lat, float lon, float alt, long timestamp) :
                lat(lat), lon(lon), alt(alt),
                timestamp(timestamp) {}

        [[nodiscard]] JsonVariant toJson() const override {
            DynamicJsonDocument doc(1024);
            doc["lat"] = this->lat;
            doc["lon"] = this->lon;
            doc["alt"] = this->alt;
            return doc.as<JsonVariant>();
        }

        bool serialize(std::string &buffer) const override {
            JsonVariant doc = toJson();
            if (serializeJson(doc, buffer) > 0)
                return true;
            else
                return false;
        }

        float lat;
        float lon;
        float alt;
        long timestamp;
    };

    struct Message : Serializable {
        Message(long trackerId, size_t visitedWaypoints, GPSCoordinates coordinates) :
                trackerId(trackerId),
                visitedWaypoints(visitedWaypoints),
                coordinates(std::move(coordinates)) {}

        bool serialize(std::string &buffer) const override {
            std::string dbg;
            serializeJson(toJson().getMember("coordinates"), dbg);
            if (serializeJson(toJson(), buffer) > 0) return true;
            else return false;
        }

        [[nodiscard]] JsonVariant toJson() const override {
            DynamicJsonDocument doc(1024);
            doc["tracker_id"] = this->trackerId;
            doc["timestamp"] = coordinates.timestamp;
            doc["visited_waypoints"] = this->visitedWaypoints;
            JsonObject coords = doc.createNestedObject("coordinates");
            coords.set(coordinates.toJson().as<JsonObject>());
            return doc.as<JsonVariant>();
        }

        long trackerId;
        size_t visitedWaypoints;
        GPS_TRACKER::GPSCoordinates coordinates;
    };
}

#endif //LIGHTWEIGHT_GPS_TRACKER_PROTOCOL_H

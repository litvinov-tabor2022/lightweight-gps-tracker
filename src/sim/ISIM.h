#ifndef LIGHTWEIGHT_GPS_TRACKER_ISIM_H
#define LIGHTWEIGHT_GPS_TRACKER_ISIM_H

#include "ArduinoJson.h"

struct GPS_COORDINATES {
    bool serialize(uint8_t *buffer, size_t buffer_size) const {
        DynamicJsonDocument doc(1024);
        doc["lat"] = this->lat;
        doc["lon"] = this->lon;
        doc["alt"] = this->alt;
        doc["timestamp"] = this->timestamp;
        if (serializeJson(doc, buffer, buffer_size) <= 0)
            return false;
        else
            return true;
    }

    float lat;
    float lon;
    float alt;
    long timestamp;
};

class ISIM {
public:
    enum STATUS_CODE {
        Ok,
        MODEM_INIT_FAILED,
        GSM_CONNECTION_ERROR,
        GPS_CONNECTION_ERROR,
        READ_GPS_COORDINATES_FAILED,
        MODEM_NOT_CONNECTED,
        UNKNOWN_ERROR
    };

    virtual ISIM::STATUS_CODE init() = 0;

    virtual ISIM::STATUS_CODE sendData(JsonDocument *data) = 0;

    virtual STATUS_CODE actualPosition(GPS_COORDINATES &coordinates) = 0;
};

#endif //LIGHTWEIGHT_GPS_TRACKER_ISIM_H

#ifndef LIGHTWEIGHT_GPS_TRACKER_ISIM_H
#define LIGHTWEIGHT_GPS_TRACKER_ISIM_H

#include "ArduinoJson.h"
#include "Protocol.h"

namespace MODEM {
    class ISIM {
    public:
        enum STATUS_CODE {
            Ok,
            MODEM_INIT_FAILED,
            GSM_CONNECTION_ERROR,
            GPS_CONNECTION_ERROR,
            READ_GPS_COORDINATES_FAILED,
            MODEM_NOT_CONNECTED,
            GPS_ACCURACY_TOO_LOW,
            GPS_COORDINATES_OUT_OF_RANGE,
            UNKNOWN_ERROR
        };

        virtual ISIM::STATUS_CODE init() = 0;

        virtual ISIM::STATUS_CODE sendData(JsonDocument *data) = 0;

        virtual STATUS_CODE actualPosition(GPS_TRACKER::GPSCoordinates *coordinates) = 0;
    };
}
#endif //LIGHTWEIGHT_GPS_TRACKER_ISIM_H

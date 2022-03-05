#ifndef LIGHTWEIGHT_GPS_TRACKER_SIM868_H
#define LIGHTWEIGHT_GPS_TRACKER_SIM868_H

#include "ISIM.h"

#define TINY_GSM_MODEM_SIM868
#define TINY_GSM_DEBUG Serial

#include "TinyGSM.h"
#include "TinyGsmClient.h"
#include "SSLClient.h"
#include "PubSubClient.h"
#include "Tasker.h"
#include "Configuration.h"
#include <mutex>


namespace GPS_TRACKER {
    /**
     * Class for interacting with SIM868 module.
     * Call init() method before you start calling others.
     *
     * WIRING: sim module must be connected with ESP32 via Serial2.
     * */
    class SIM868 : ISIM {
    public:
        SIM868();

        explicit SIM868(GPS_TRACKER::Configuration &config) : configuration(config) {};

        ISIM::STATUS_CODE sendData(JsonDocument *data) override;

        ISIM::STATUS_CODE sendData(const std::string &data);

        /**
         * @return `MODEM_NOT_CONNECTED` if sim module is not connected, `Ok` if position read successfully, otherwise `UNKNOWN_ERROR`
         * */
        STATUS_CODE actualPosition(GPS_COORDINATES &coordinates) override;

        /**
         * @return `Ok` if initialization was successful
         * */
        ISIM::STATUS_CODE init() override;

        bool isConnected();

    private:
        /**
         * This is blocking function! It blocks thread until the network connection is established.
         * */
        bool connectGPRS();

        bool connectGPS();

        bool connectToMqtt();

        bool reconnect();

        std::recursive_mutex gsm_mutex; // all operations with
        TinyGsm modem = TinyGsm(Serial2);
        TinyGsmClient gsmClient = TinyGsmClient(modem);
        SSLClient gsmClientSSL = SSLClient(&gsmClient);
        PubSubClient mqttClient = PubSubClient(gsmClientSSL);
        GPS_TRACKER::Configuration configuration;
    };
}

#endif //LIGHTWEIGHT_GPS_TRACKER_SIM868_H

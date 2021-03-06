#ifndef LIGHTWEIGHT_GPS_TRACKER_SIM7000G_H
#define LIGHTWEIGHT_GPS_TRACKER_SIM7000G_H

#include "ISIM.h"

#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_DEBUG Serial

#include <TinyGSM.h>
#include "Protocol.h"
#include <TinyGsmClient.h>
#include "SSLClient.h"
#include "Tasker.h"
#include "Configuration.h"
#include "StreamDebugger.h"
#include "StateManager.h"
#include "logger/Logger.h"
#include "MqttClient.h"
#include <ArduinoHttpClient.h>
#include <mutex>

namespace GPS_TRACKER {
    using namespace MODEM;

    /**
     * Class for interacting with SIM7000G module.
     * Call init() method before you start calling others.
     * */
    class SIM7000G : public ISIM {
    public:
        explicit SIM7000G(Logging::Logger *logger, GPS_TRACKER::Configuration &config,
                          GPS_TRACKER::StateManager *stateManager) : logger(logger),
                                                                     configuration(config),
                                                                     stateManager(stateManager) {};

        STATUS_CODE sendData(JsonDocument *data) override;

        /**
         * @return `MODEM_NOT_CONNECTED` if sim module is not connected, `Ok` if position read successfully, otherwise `UNKNOWN_ERROR`
         * */
        STATUS_CODE actualPosition(GPSCoordinates *coordinates) override;

        /**
         * @return `Ok` if initialization was successful
         * */
        STATUS_CODE init() override;

        bool isConnected();

        bool isGpsConnected();

        MODEM::STATUS_CODE sendActPosition() override;

        Timestamp getActTime();

        MODEM::STATUS_CODE sleep() override;

        MODEM::STATUS_CODE wakeUp() override;

        void powerOff();

    private:
        static double battery();

        double batteryPercentage();

        /**
         * This is blocking function! It blocks thread until the network connection is established.
         * */
        bool connectGPRS();

        /**
         * This is blocking function! It blocks thread until the GSP position is fixed.
         * */
        bool connectGPS();

        bool reconnect();

        /**
         * Downloads XTRA file. Call this once ever every three days.
         * */
        void fastFix();

        void setGPSAccuracy(int meters);

        void hotStart();

        void warmStart();

        /**
         * Call this after the XTRA file is updated.
         * */
        void coldStart();

        Logging::Logger *logger;
        StreamDebugger *debugger = new StreamDebugger(SerialAT, Serial);
        TinyGsm modem = TinyGsm(SerialAT);
        TinyGsmClient gsmClient = TinyGsmClient(modem, 0);
        TinyGsmClient gsmClient1 = TinyGsmClient(modem, 1);
        SSLClient gsmClientSSL = SSLClient(&gsmClient);
        SSLClient gsmClientSSL1 = SSLClient(&gsmClient1);
        MqttClient mqttClient;
        HttpClient http = HttpClient(gsmClientSSL1, SERVER_NAME.c_str(), 443);
        GPS_TRACKER::Configuration configuration;
        GPS_TRACKER::StateManager *stateManager;
        double batteryFullyChargedLimit = 4200;
        double batteryDischargeVoltage = 2700;
    };
}

#endif //LIGHTWEIGHT_GPS_TRACKER_SIM7000G_H

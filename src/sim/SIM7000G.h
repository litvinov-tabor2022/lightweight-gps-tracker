#ifndef LIGHTWEIGHT_GPS_TRACKER_SIM7000G_H
#define LIGHTWEIGHT_GPS_TRACKER_SIM7000G_H

#include "ISIM.h"

#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_DEBUG Serial

#include "TinyGSM.h"
#include "Protocol.h"
#include "TinyGsmClient.h"
#include "SSLClient.h"
#include "PubSubClient.h"
#include "Tasker.h"
#include "Configuration.h"
#include "StreamDebugger.h"
#include "StateManager.h"
#include "logger/Logger.h"
#include <mutex>

namespace GPS_TRACKER {
#define uS_TO_S_FACTOR 1000000ULL  // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP  60          // Time ESP32 will go to sleep (in seconds)

#define SerialAT Serial1
#define UART_BAUD   9600
#define PIN_DTR     25
#define PIN_TX      27
#define PIN_RX      26
#define PWR_PIN     4

#define SD_MISO     2
#define SD_MOSI     15
#define SD_SCLK     14
#define SD_CS       13
#define LED_PIN     12

    class SIM7000G : MODEM::ISIM {
/**
     * Class for interacting with SIM868 module.
     * Call init() method before you start calling others.
     *
     * WIRING: sim module must be connected with ESP32 via Serial2.
     * */
    public:
        explicit SIM7000G(Logging::Logger *logger, GPS_TRACKER::Configuration &config) : logger(logger),
                                                                                configuration(config) {};

        ISIM::STATUS_CODE sendData(JsonDocument *data) override;

        ISIM::STATUS_CODE sendData(const std::string &data);

        /**
         * @return `MODEM_NOT_CONNECTED` if sim module is not connected, `Ok` if position read successfully, otherwise `UNKNOWN_ERROR`
         * */
        STATUS_CODE actualPosition(GPSCoordinates *coordinates) override;

        /**
         * @return `Ok` if initialization was successful
         * */
        ISIM::STATUS_CODE init() override;

        bool isConnected();

        bool isGpsConnected();

        bool isMqttConnected();

    private:
        /**
         * This is blocking function! It blocks thread until the network connection is established.
         * */
        bool connectGPRS();

        bool connectGPS();

        bool connectToMqtt();

        bool reconnect();

        bool isModemConnected() {
            std::lock_guard<std::mutex> lg(mut);
            return modem.isNetworkConnected() && modem.isGprsConnected();
        }

        void fastFix();

        Logging::Logger *logger;
        std::recursive_mutex gsm_mutex; // all operations with
        std::mutex mut; // all operations with
        StreamDebugger *debugger = new StreamDebugger(SerialAT, Serial);
        TinyGsm modem = TinyGsm(SerialAT);
        TinyGsmClient gsmClient = TinyGsmClient(modem);
        SSLClient gsmClientSSL = SSLClient(&gsmClient);
        PubSubClient mqttClient = PubSubClient(gsmClientSSL);
        GPS_TRACKER::Configuration configuration;
    };
}

#endif //LIGHTWEIGHT_GPS_TRACKER_SIM7000G_H

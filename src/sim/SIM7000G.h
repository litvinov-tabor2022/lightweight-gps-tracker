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
    using namespace MODEM;

#define SerialAT    Serial1
#define UART_BAUD   115200
#define PIN_DTR     25
#define PIN_TX      27
#define PIN_RX      26
#define PWR_PIN     4

#define SD_MISO     2
#define SD_MOSI     15
#define SD_SCLK     14
#define SD_CS       13
#define LED_PIN     12
#define PIN_ADC_BAT 35
#define PIN_ADC_SOLAR 36
#define ADC_BATTERY_LEVEL_SAMPLES 100

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

        STATUS_CODE sendData(const std::string &data);

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

        bool isMqttConnected();

        MODEM::STATUS_CODE sendActPosition();

        Timestamp getActTime();

        void sleep();

        void wakeUp();

        void powerOff();

    private:
        static double battery();

        double batteryPercentage() const;

        /**
         * This is blocking function! It blocks thread until the network connection is established.
         * */
        bool connectGPRS();

        /**
         * This is blocking function! It blocks thread until the GSP position is fixed.
         * */
        bool connectGPS();

        /**
         * This is blocking function! It blocks thread until the connection with MQTT broker is established.
         * */
        bool connectToMqtt();

        bool reconnect();

        bool isModemConnected() {
            std::lock_guard<std::mutex> lg(mut);
            return modem.isNetworkConnected() && modem.isGprsConnected();
        }

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
        std::recursive_mutex gsm_mutex; // all operations with
        std::mutex mut; // all operations with
        StreamDebugger *debugger = new StreamDebugger(SerialAT, Serial);
        TinyGsm modem = TinyGsm(SerialAT);
        TinyGsmClient gsmClient = TinyGsmClient(modem);
        SSLClient gsmClientSSL = SSLClient(&gsmClient);
        PubSubClient mqttClient = PubSubClient(gsmClientSSL);
        GPS_TRACKER::Configuration configuration;
        GPS_TRACKER::StateManager *stateManager;
        double batteryFullyChargedLimit = 4200;
        double batteryDischargeVoltage = 2700;
    };
}

#endif //LIGHTWEIGHT_GPS_TRACKER_SIM7000G_H

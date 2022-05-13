#ifndef LIGHTWEIGHT_GPS_TRACKER_CONSTANTS_H
#define LIGHTWEIGHT_GPS_TRACKER_CONSTANTS_H

#include <string>

static const std::string  SSID = "tracker";
static const std::string  PASSWORD = "tracker123";

#define SD_MISO     2
#define SD_MOSI     15
#define SD_SCLK     14
#define SD_CS       13

#define SerialAT    Serial1
#define UART_BAUD   115200
#define PIN_DTR     25
#define PIN_TX      27
#define PIN_RX      26
#define PWR_PIN     4

#define LED_PIN     12
#define PIN_ADC_BAT 35
#define PIN_ADC_SOLAR 36
#define ADC_BATTERY_LEVEL_SAMPLES 100


#define uS_TO_S_FACTOR 1000000
static const std::string CONFIG_PATH = "/config.json";
static const int DEFAULT_VOLUME = 100; // %

static const std::string SERVER_NAME = "";

#endif //LIGHTWEIGHT_GPS_TRACKER_CONSTANTS_H

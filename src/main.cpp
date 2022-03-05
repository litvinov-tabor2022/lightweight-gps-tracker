#include <Arduino.h>
#include "Configuration.h"
#include "sim/SIM868.h"
#include "SPIFFS.h"
#include "audio/SDAudioPlayer.h"
#include "audio/AudioPlayerFactory.h"

GPS_TRACKER::SIM868 *sim;
GPS_TRACKER::Configuration *configuration;
SDAudioPlayer *audioPlayer;

void setup() {
    Serial.begin(115200);
    Serial.println("Initialization start....");
    SPIFFS.begin();

    // ------ CONFIGURATION
    configuration = new GPS_TRACKER::Configuration();
    if (!configuration->read()) {
        Serial.println("GPS TRACKER INITIALIZATION FAILED!!!!!!!!");
    } else {
        Serial.printf("Configuration loaded.\n\t # waypoints: %d\n", configuration->WAYPOINTS.size());
    }

    // ------ GSM/GPS
    sim = new GPS_TRACKER::SIM868(*configuration);
    ISIM::STATUS_CODE initRes = sim->init();
    if (initRes != ISIM::Ok) {
        Serial.printf("Modem initialization failed> %u\n", initRes);
    }

    // ------ AUDIO
    audioPlayer = AudioPlayerFactory::mp3spiffsAudioPlayer();
    audioPlayer->play();
}

void loop() {
    // everything is handled by tasks
}
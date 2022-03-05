#ifndef LIGHTWEIGHT_GPS_TRACKER_AUDIOPLAYERFACTORY_H
#define LIGHTWEIGHT_GPS_TRACKER_AUDIOPLAYERFACTORY_H

#include "AudioFileSourceSD.h"
#include "SDAudioPlayer.h"
#include "Constants.h"
#include "AudioFileSourceSPIFFS.h"

class AudioPlayerFactory {
public:
    static SDAudioPlayer *mp3sdAudioPlayer() {
        AudioOutputI2S out = AudioOutputI2S();
        AudioGeneratorMP3 mp3 = AudioGeneratorMP3();
        AudioFileSourceSD source = AudioFileSourceSD("/");
        return new SDAudioPlayer(&mp3, &out, &source, (DEFAULT_VOLUME / 100.0));
    }

    static SDAudioPlayer *mp3spiffsAudioPlayer() {
        AudioOutputI2S out = AudioOutputI2S();
        AudioGeneratorMP3 mp3 = AudioGeneratorMP3();
        AudioFileSourceSPIFFS source = AudioFileSourceSPIFFS("/");
        return new SDAudioPlayer(&mp3, &out, &source, (DEFAULT_VOLUME / 100.0));
    }
};

#endif //LIGHTWEIGHT_GPS_TRACKER_AUDIOPLAYERFACTORY_H

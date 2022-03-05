#ifndef LIGHTWEIGHT_GPS_TRACKER_SDAUDIOPLAYER_H
#define LIGHTWEIGHT_GPS_TRACKER_SDAUDIOPLAYER_H

#include <Tasker.h>
#include <queue>
#include <mutex>
#include <utility>
#include "AudioFileSourceID3.h"
#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

struct Sound {
    explicit Sound(std::string p, bool i = false) {
        path = std::move(p);
        uninterruptible = i;
    }

    std::string path;
    bool uninterruptible;
};

class SDAudioPlayer {
public:
    SDAudioPlayer(AudioGenerator *audioGenerator, AudioOutput *audioOutput, AudioFileSource *fileSource, float volume);

    void play();

    void enqueueFile(const std::string &path, bool uninterruptible = false);

    void playFile(const std::string &path, bool uninterruptible = false);

    /**
     * @param newVolume New volume in % [range: 0 to 200]
     * */
    bool setVolume(int newVolume);

private:

    void playNext();

    void selectFile(const Sound &sound);

    std::mutex queueMutex; // access to queue must be exclusive
    AudioGenerator *audioGenerator;
    AudioOutput *audioOutput;
    AudioFileSource *fileSource;
    float volume;
    bool playingUninterruptible = false;
    std::queue<Sound> fileQueue;
};

#endif //LIGHTWEIGHT_GPS_TRACKER_SDAUDIOPLAYER_H

#ifndef LIGHTWEIGHT_GPS_TRACKER_PLAYER_H
#define LIGHTWEIGHT_GPS_TRACKER_PLAYER_H

#include <Tasker.h>
#include <queue>
#include <mutex>
#include <utility>
#include "AudioFileSourceID3.h"
#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include "StateManager.h"
#include "logger/Logger.h"

namespace AudioPlayer {
    struct Sound {
        explicit Sound(std::string p, bool i = false) {
            path = std::move(p);
            uninterruptible = i;
        }

        std::string path;
        bool uninterruptible;
    };

    class Player {
    public:
        Player(Logging::Logger *logger, AudioGenerator *audioGenerator, AudioOutput *audioOutput, AudioFileSource *fileSource,
               float volume);

        void init();

        void play();

        void stop();

        void enqueueFile(const std::string &path, bool uninterruptible = false);

        void playFile(const std::string &path, bool uninterruptible = false);

        /**
         * @param newVolume New volume in % [range: 0 to 200]
         * */
        bool setVolume(int newVolume);

    private:

        void playNext();

        void selectFile(const Sound &sound);

        Logging::Logger *logger;
        std::mutex queueMutex; // access to queue must be exclusive
        AudioGenerator *audioGenerator;
        AudioOutput *audioOutput;
        AudioFileSource *fileSource;
        float volume;
        bool playingUninterruptible = false;
        std::queue<Sound> fileQueue;
        bool isPlaying = false;
    };
}

#endif //LIGHTWEIGHT_GPS_TRACKER_PLAYER_H

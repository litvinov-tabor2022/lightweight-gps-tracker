#include "Player.h"

AudioPlayer::Player::Player(Logging::Logger *logger,
                            AudioGenerator *audioGenerator,
                            AudioOutput *audioOutput,
                            AudioFileSource *fileSource,
                            float volume) :
        logger(logger),
        audioGenerator(audioGenerator),
        audioOutput(audioOutput),
        fileSource(fileSource),
        volume(volume) {
    init();
}


void AudioPlayer::Player::init() {
    this->audioGenerator->begin(this->fileSource, this->audioOutput);
}

/**
 * Routine for reading and reproducing samples.
 * */
void AudioPlayer::Player::play() {
    SoundTasker.loop("sound", [this] {
        if (!audioGenerator->loop()) {
            audioGenerator->stop();
            playingUninterruptible = false;
        }
        if (!audioGenerator->isRunning()) {
            audioGenerator->stop();
            audioGenerator->desync();
            playingUninterruptible = false;
            if (isPlaying) {
                esp_restart();
            }
            playNext();
            playNext();
        }
    });
}

bool AudioPlayer::Player::setVolume(int newVolume) {
    if (newVolume < 0 || newVolume > 200) {
        logger->printf(Logging::INFO, "Volume is out of range (value %d)\n", newVolume);
        return false;
    }
    volume = (float) newVolume / 100;
    audioOutput->SetGain(volume);
    logger->printf(Logging::INFO, "Volume set to %f\n", volume);
    return true;
}

void AudioPlayer::Player::selectFile(const Sound &sound) {
    playingUninterruptible = sound.uninterruptible;
    fileSource->open(sound.path.c_str());
    if (!audioGenerator->isRunning()) {
        audioGenerator->begin(fileSource, audioOutput);
    }
}

void AudioPlayer::Player::enqueueFile(const std::string &path, bool uninterruptible) {
    std::lock_guard<std::mutex> lock(queueMutex);
    fileQueue.push(Sound(path, uninterruptible));
}

void AudioPlayer::Player::playNext() {
    std::lock_guard<std::mutex> lock(queueMutex);

    if (fileQueue.empty()) return;
    logger->printf(Logging::INFO, "Playing next file: %s\n", fileQueue.front().path.c_str());
    selectFile(fileQueue.front());
    fileQueue.pop();
    isPlaying = true;
}

void AudioPlayer::Player::playFile(const std::string &path, bool uninterruptible) {
    if (playingUninterruptible) {
        std::lock_guard<std::mutex> lock(queueMutex);
        logger->printf(Logging::INFO, "File enqueued to front: %s\n", path.c_str());
        fileQueue.push(Sound(path, uninterruptible));
    } else {
        selectFile(Sound(path, uninterruptible));
    }
}

void AudioPlayer::Player::stop() {
    this->audioGenerator->desync();
    this->audioGenerator->stop();
    this->audioOutput->stop();
}

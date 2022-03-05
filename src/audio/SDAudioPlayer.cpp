#include "SDAudioPlayer.h"

SDAudioPlayer::SDAudioPlayer(AudioGenerator *audioGenerator, AudioOutput *audioOutput, AudioFileSource *fileSource,
                             float volume) : audioGenerator(audioGenerator), audioOutput(audioOutput),
                                             fileSource(fileSource), volume(volume) {
    this->audioGenerator->begin(this->fileSource, this->audioOutput);
}

/**
 * Routine for reading and reproducing samples.
 * */
void SDAudioPlayer::play() {
    SoundTasker.loop("sound", [this] {
        if (!audioGenerator->loop()) {
            audioGenerator->stop();
            playingUninterruptible = false;
        }
        if (!audioGenerator->isRunning()) {
            audioGenerator->stop();
            audioGenerator->desync();
            playingUninterruptible = false;
            playNext();
        }
    });
}

bool SDAudioPlayer::setVolume(int newVolume) {
    if (newVolume < 0 || newVolume > 200) {
        Serial.printf("Volume is out of range (value %d)\n", newVolume);
        return false;
    }
    volume = (float) newVolume / 100;
    audioOutput->SetGain(volume);
    Serial.printf("Volume set to %f\n", volume);
    return true;
}

void SDAudioPlayer::selectFile(const Sound &sound) {
    playingUninterruptible = sound.uninterruptible;
    fileSource->open(sound.path.c_str());
    if (!audioGenerator->isRunning()) {
        audioGenerator->begin(fileSource, audioOutput);
    }
}

void SDAudioPlayer::enqueueFile(const std::string &path, bool uninterruptible) {
    std::lock_guard<std::mutex> lock(queueMutex);
    fileQueue.push(Sound(path, uninterruptible));
}

void SDAudioPlayer::playNext() {
    std::lock_guard<std::mutex> lock(queueMutex);

    if (fileQueue.empty()) return;
    Serial.printf("Playing next file: %s\n", fileQueue.front().path.c_str());
    selectFile(fileQueue.front());
    fileQueue.pop();
}

void SDAudioPlayer::playFile(const std::string &path, bool uninterruptible) {
    if (playingUninterruptible) {
        std::lock_guard<std::mutex> lock(queueMutex);
        Serial.printf("File enqueued to front: %s\n", path.c_str());
        fileQueue.push(Sound(path, uninterruptible));
    } else {
        selectFile(Sound(path, uninterruptible));
    }
}

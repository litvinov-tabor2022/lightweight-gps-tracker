#include <Arduino.h>

#include "Tasker.h"

Tasker::Tasker(int core) {
    this->core = core;
}

[[noreturn]] static void executeLoop(void *uarg) {
    auto *fnc_ptr = static_cast<std::function<void(void)> *>(uarg);
    auto fnc = std::move(*fnc_ptr);
    delete fnc_ptr;

    for (;;) {
        fnc();
    }
}

void execute(void *uarg) {
    auto *fnc_ptr = static_cast<std::function<void(void)> *>(uarg);
    auto fnc = std::move(*fnc_ptr);
    delete fnc_ptr;

    fnc();
}

void Tasker::loop(const char *name, std::function<void(void)> op) const {
    auto *fnc = new std::function<void(void)>{[op] {
        op();
        Tasker::yield();
    }};
    xTaskCreatePinnedToCore(executeLoop, name, 10000, static_cast<void *>(fnc), 1, NULL, core);
}

void Tasker::loopEvery(const char *name, int millis, std::function<void(void)> op) const {
    auto *fnc = new std::function<void(void)>{[op, millis] {
        op();
        Tasker::yield();
        Tasker::sleep(millis);
    }};
    xTaskCreatePinnedToCore(executeLoop, name, 10000, static_cast<void *>(fnc), 1, NULL, core);
}

void Tasker::once(const char *name, std::function<void(void)> op) const {
    auto *fnc = new std::function<void(void)>{[op] {
        op();
        vTaskDelete(NULL);
    }};
    xTaskCreatePinnedToCore(execute, name, 10000, static_cast<void *>(fnc), 1, NULL, core);
}

void Tasker::sleep(int millis) {
    taskYIELD();
    vTaskDelay(pdMS_TO_TICKS(millis));
}

void Tasker::yield() {
    Tasker::sleep(1);
}


Tasker SoundTasker(0);
Tasker DefaultTasker(1);

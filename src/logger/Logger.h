#ifndef LIGHTWEIGHT_GPS_TRACKER_LOGGER_H
#define LIGHTWEIGHT_GPS_TRACKER_LOGGER_H

#include "Arduino.h"

namespace Logging {
    enum Level {
        DEBUG, INFO, WARNING, ERROR
    };

    class NullStream : public Stream {
    public:
        size_t write(uint8_t uint8) override {
            return 0;
        }

        int available() override {
            return 0;
        }

        int read() override {
            return 0;
        }

        int peek() override {
            return 0;
        }

        void flush() override {

        }
    };

    class Logger{
    public:
        static Logger serialLogger(Level level = WARNING) {
            return {&Serial, level};
        }

        static Logger nullLogger() {
            return {new NullStream};
        }

        void print(Level level, const String &string) {
            print(level, string.c_str());
        }

        void print(Level level, const char *string) {
            if (level >= minLevel)
                logger->printf("%s %s", levelToString(level).c_str(), string);
        }

        void println(Level level, const String &string) {
            if (level >= minLevel)
                println(level, string.c_str());
        }

        void println(Level level, const char *string) {
            if (level >= minLevel)
                logger->printf("%s %s\n", levelToString(level).c_str(), string);
        }

        template<typename T, typename... Targs>
        void printf(Level level, const char *format, T value, Targs... Fargs) {
            if (level >= minLevel) {
                logger->print(levelToString(level) + ' ');
                rec_printf(format, value, Fargs...);
            }
        }

    private:
        Logger(Stream *logger) : logger(logger) {}

        Logger(Stream *logger, Level level) : logger(logger), minLevel(level) {}

        void rec_printf(const char *format) {
            logger->print(format);
        }

        template<typename T, typename... Targs>
        void rec_printf(const char *format, T value, Targs... Fargs) {
            for (; *format != '\0'; format++) {
                if (*format == '%') {
                    logger->print(value);
                    rec_printf(format + 1, Fargs...);
                    return;
                }
                logger->print(*format);
            }
        }

        static String levelToString(Level level) {
            switch (level) {
                case DEBUG:
                    return "[DBG]";
                case INFO:
                    return "[INFO]";
                case WARNING:
                    return "[WARN]";
                case ERROR:
                    return "[ERR]";
                default:
                    return "[???]";
            }
        }

        Stream *logger;
        Level minLevel;
    };
}


#endif //LIGHTWEIGHT_GPS_TRACKER_LOGGER_H

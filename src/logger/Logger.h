#ifndef LOGGING_LOGGER_H
#define LOGGING_LOGGER_H

#include "Arduino.h"
#include "Utils.h"
#include <TelnetStream.h>

namespace {
    class NullStream : public Print {
    public:
        size_t write(uint8_t uint8) override {
            return 0;
        }
    };

    class StreamSplitter : public Print {
    public:
        void addStream(Stream *newStream) {
            streams.push_back(newStream);
        }

        size_t write(uint8_t uint8) override {
            size_t writtenBytes = 0;
            for (auto stream: streams) {
                writtenBytes += stream->write(uint8);
            }
            return writtenBytes;
        }

//        void flush() override {
//            for (auto stream: streams) {
//                stream->flush();
//            }
//        }

    private:
        std::vector<Stream *> streams;
    };
}

namespace Logging {
    enum Level {
        DEBUG, INFO, WARNING, ERROR
    };

    class Logger : public Print {
    public:
        static Logger *serialLogger(Level level = WARNING) {
            return new Logger{&Serial, level};
        }

        static Logger *nullLogger() {
            return new Logger{new NullStream};
        }

        static Logger *telnetLogger(Level level = WARNING) {
            auto telnetStream = new TelnetStreamClass(23);
            telnetStream->begin(23);
            return new Logger{telnetStream, level};
        }

        /**
         * WiFi must be initialized before calling this method!
         */
        static Logger *serialAndTelnetLogger(Level level = WARNING) {
            auto telnetStream = new TelnetStreamClass(23);
            telnetStream->begin(23);
            auto splitter = new StreamSplitter();
            splitter->addStream(&Serial);
            splitter->addStream(telnetStream);
            return new Logger{splitter, level};
        }

        static Logger *fileLogger(File *file, Level level = WARNING) {
            auto splitter = new StreamSplitter();
            splitter->addStream(&Serial);
            splitter->addStream(file);
            return new Logger{splitter, level};
        }

        void print(Level level, const String &string) {
            print(level, string.c_str());
        }

        void print(Level level, const char *string) {
            if (level >= minLevel) {
                time_t now;
                time(&now);
                logger->printf("T: %ld ", now);
                logger->printf("%s %s", levelToString(level, tag).c_str(), string);
//                logger->flush();
            }
        }

        void println(Level level, const String &string) {
            if (level >= minLevel) {
                time_t now;
                time(&now);
                logger->printf("T: %ld ", now);
                println(level, string.c_str());
            }
        }

        void println(Level level, const char *string) {
            if (level >= minLevel) {
                time_t now;
                time(&now);
                logger->printf("T: %ld ", now);
                logger->printf("%s %s\n", levelToString(level, tag).c_str(), string);
//                logger->flush();
            }
        }

        template<typename T, typename... Targs>
        void printf(Level level, const char *format, T value, Targs... Fargs) {
            if (level >= minLevel) {
                time_t now;
                time(&now);
                logger->printf("T: %ld ", now);
                logger->print(levelToString(level, tag) + ' ');
                rec_printf(format, value, Fargs...);
            }
        }

    private:
        size_t write(uint8_t uint8) override {
            return logger->write(uint8);
        }

        Logger(Print *logger) : logger(logger) {}

        Logger(Print *logger, Level level) : logger(logger), minLevel(level) {}

        void rec_printf(const char *format) {
            logger->print(format);
        }

        template<typename T, typename... Targs>
        void rec_printf(const char *format, T value, Targs... Fargs) {
            for (; *format != '\0'; format++) {
                if (*format == '%') {
                    format++;
                    logger->print(value);
                    rec_printf(format + 1, Fargs...);
                    return;
                }
                logger->print(*format);
            }
//            logger->flush();
        }

        static String levelToString(Level level, const String& prefix = "") {
            switch (level) {
                case DEBUG:
                    return prefix + " [DBG]";
                case INFO:
                    return prefix + " [INFO]";
                case WARNING:
                    return prefix + " [WARN]";
                case ERROR:
                    return prefix + " [ERR]";
                default:
                    return prefix + " [???]";
            }
        }

        String tag = Utils::randomString(8);
        Print *logger;
        Level minLevel;
    };
}


#endif //LOGGING_LOGGER_H

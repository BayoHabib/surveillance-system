#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <string>
#include <cstdlib>

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

class Logger {
public:
    static LogLevel GetLogLevel() {
        return GetLevelRef();
    }

    static void SetLogLevel(LogLevel new_level) {
        static LogLevel& level_ref = GetLevelRef();
        level_ref = new_level;
    }

    template<typename... Args>
    static void Debug(Args&&... args) {
        if (GetLogLevel() <= LogLevel::DEBUG) {
            std::cerr << "[DEBUG] ";
            (std::cerr << ... << args);
            std::cerr << std::endl;
        }
    }

    template<typename... Args>
    static void Info(Args&&... args) {
        if (GetLogLevel() <= LogLevel::INFO) {
            std::cerr << "[INFO] ";
            (std::cerr << ... << args);
            std::cerr << std::endl;
        }
    }

    template<typename... Args>
    static void Warn(Args&&... args) {
        if (GetLogLevel() <= LogLevel::WARN) {
            std::cerr << "[WARN] ";
            (std::cerr << ... << args);
            std::cerr << std::endl;
        }
    }

    template<typename... Args>
    static void Error(Args&&... args) {
        if (GetLogLevel() <= LogLevel::ERROR) {
            std::cerr << "[ERROR] ";
            (std::cerr << ... << args);
            std::cerr << std::endl;
        }
    }

private:
    static LogLevel& GetLevelRef() {
        static LogLevel level = InitLogLevel();
        return level;
    }

    static LogLevel InitLogLevel() {
        const char* env = std::getenv("LOG_LEVEL");
        if (env == nullptr) {
            return LogLevel::INFO; // Default to INFO
        }
        
        std::string level(env);
        if (level == "DEBUG") return LogLevel::DEBUG;
        if (level == "INFO") return LogLevel::INFO;
        if (level == "WARN") return LogLevel::WARN;
        if (level == "ERROR") return LogLevel::ERROR;
        
        return LogLevel::INFO;
    }
};

// Convenience macros
#define LOG_DEBUG(...) Logger::Debug(__VA_ARGS__)
#define LOG_INFO(...) Logger::Info(__VA_ARGS__)
#define LOG_WARN(...) Logger::Warn(__VA_ARGS__)
#define LOG_WARNING(...) Logger::Warn(__VA_ARGS__)  // Alias for compatibility
#define LOG_ERROR(...) Logger::Error(__VA_ARGS__)

#endif // LOGGER_H

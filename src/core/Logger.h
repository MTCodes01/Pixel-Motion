#pragma once

#include <string>
#include <fstream>
#include <mutex>

namespace PixelMotion {

/**
 * Simple logging system
 * Logs to both file and debug output
 */
class Logger {
public:
    enum class Level {
        Info,
        Warning,
        Error
    };

    static void Initialize();
    static void Shutdown();

    static void Info(const std::string& message);
    static void Warning(const std::string& message);
    static void Error(const std::string& message);

private:
    static void Log(Level level, const std::string& message);
    static std::string GetTimestamp();
    static std::string LevelToString(Level level);

    static std::ofstream s_logFile;
    static std::mutex s_mutex;
    static bool s_initialized;
};

} // namespace PixelMotion

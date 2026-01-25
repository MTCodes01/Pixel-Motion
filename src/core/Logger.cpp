#include "Logger.h"

#include <Windows.h>
#include <shlobj.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace PixelMotion {

std::ofstream Logger::s_logFile;
std::mutex Logger::s_mutex;
bool Logger::s_initialized = false;

void Logger::Initialize() {
    if (s_initialized) return;

    // Get AppData\Local\PixelMotion\logs directory
    wchar_t* localAppData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData))) {
        std::filesystem::path logDir = std::filesystem::path(localAppData) / L"PixelMotion" / L"logs";
        CoTaskMemFree(localAppData);

        // Create directory if it doesn't exist
        std::filesystem::create_directories(logDir);

        // Create log file with timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_s(&tm, &time_t);

        std::wostringstream filename;
        filename << L"PixelMotion_"
                 << std::put_time(&tm, L"%Y%m%d_%H%M%S")
                 << L".log";

        std::filesystem::path logPath = logDir / filename.str();
        s_logFile.open(logPath, std::ios::out | std::ios::app);
    }

    s_initialized = true;
}

void Logger::Shutdown() {
    if (s_logFile.is_open()) {
        s_logFile.close();
    }
    s_initialized = false;
}

void Logger::Info(const std::string& message) {
    Log(Level::Info, message);
}

void Logger::Warning(const std::string& message) {
    Log(Level::Warning, message);
}

void Logger::Error(const std::string& message) {
    Log(Level::Error, message);
}

void Logger::Log(Level level, const std::string& message) {
    std::lock_guard<std::mutex> lock(s_mutex);

    std::string timestamp = GetTimestamp();
    std::string levelStr = LevelToString(level);
    std::string fullMessage = "[" + timestamp + "] [" + levelStr + "] " + message + "\n";

    // Write to file
    if (s_logFile.is_open()) {
        s_logFile << fullMessage;
        s_logFile.flush();
    }

    // Write to debug output
    OutputDebugStringA(fullMessage.c_str());
}

std::string Logger::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm;
    localtime_s(&tm, &time_t);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string Logger::LevelToString(Level level) {
    switch (level) {
        case Level::Info:    return "INFO";
        case Level::Warning: return "WARN";
        case Level::Error:   return "ERROR";
        default:             return "UNKNOWN";
    }
}

} // namespace PixelMotion

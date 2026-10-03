#include "core/log.hpp"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

static std::ofstream logFile;
static std::mutex logMutex;

void logInit(const std::filesystem::path& logFilePath) {
    std::lock_guard<std::mutex> lock(logMutex);
    logFile.open(logFilePath, std::ios::out | std::ios::trunc);
}

void logWrite(const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);

    if (!logFile.is_open()) {
        return;
    }

    logFile << message << '\n';
    logFile.flush();
}

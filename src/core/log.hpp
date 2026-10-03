#pragma once

#include <filesystem>
#include <string>

/**
 * @brief Opens (and truncates) the log file, every later `logWrite` call appends to it.
 * @param logFilePath The path of the log file.
 */
void logInit(const std::filesystem::path& logFilePath);

/**
 * @brief A thread-safe log writer that adds a newline after each message and flushes immediately,
 * so the log stays complete even if the game crashes right after.
 * @param message The message to write.
 */
void logWrite(const std::string& message);

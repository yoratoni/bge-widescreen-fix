#pragma once

#include "types.hpp"
#include <filesystem>

/**
 * @brief Loads the fix configuration from an INI file, every missing key (or the whole file)
 * falls back to `DEFAULT_FIX_CONFIG`.
 * @param configFilePath The path of the INI file.
 * @return The loaded configuration.
 */
FixConfig configLoad(const std::filesystem::path& configFilePath);

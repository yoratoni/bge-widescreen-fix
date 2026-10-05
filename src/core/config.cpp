#include "core/config.hpp"
#include "core/constants.hpp"
#include <filesystem>
#include <windows.h>

/**
 * @brief Reads a boolean key (any non-zero integer is true).
 * @param configFilePath The path of the INI file.
 * @param section The INI section.
 * @param key The INI key.
 * @param defaultValue The value returned when the key is missing.
 * @return The value of the key.
 */
static bool readBool(const std::filesystem::path& configFilePath, const wchar_t* section, const wchar_t* key, bool defaultValue) {
    return GetPrivateProfileIntW(section, key, defaultValue ? 1 : 0, configFilePath.c_str()) != 0;
}

FixConfig configLoad(const std::filesystem::path& configFilePath) {
    FixConfig config = DEFAULT_FIX_CONFIG;

    config.isEnabled = readBool(configFilePath, CONFIG_SECTION_GENERAL, CONFIG_KEY_ENABLED, config.isEnabled);

    config.forceRenderSize
        = readBool(configFilePath, CONFIG_SECTION_FIXES, CONFIG_KEY_FORCE_RENDER_SIZE, config.forceRenderSize);
    config.fitInside = readBool(configFilePath, CONFIG_SECTION_FIXES, CONFIG_KEY_FIT_INSIDE, config.fitInside);
    config.horPlus = readBool(configFilePath, CONFIG_SECTION_FIXES, CONFIG_KEY_HOR_PLUS, config.horPlus);
    config.textFix = readBool(configFilePath, CONFIG_SECTION_FIXES, CONFIG_KEY_TEXT_FIX, config.textFix);
    config.centerFittedOffsets = readBool(
        configFilePath, CONFIG_SECTION_FIXES, CONFIG_KEY_CENTER_FITTED_OFFSETS, config.centerFittedOffsets);

    config.logFittedViews
        = readBool(configFilePath, CONFIG_SECTION_DEBUG, CONFIG_KEY_LOG_FITTED_VIEWS, config.logFittedViews);

    return config;
}

#include "core/config.hpp"
#include "core/constants.hpp"
#include <cwchar>
#include <filesystem>
#include <iterator>
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

/**
 * @brief Reads a float key, an unparsable value is treated as missing.
 * @param configFilePath The path of the INI file.
 * @param section The INI section.
 * @param key The INI key.
 * @param defaultValue The value returned when the key is missing or invalid.
 * @return The value of the key.
 */
static float readFloat(
    const std::filesystem::path& configFilePath, const wchar_t* section, const wchar_t* key, float defaultValue) {
    wchar_t buffer[64] {};
    GetPrivateProfileStringW(section, key, L"", buffer, static_cast<DWORD>(std::size(buffer)), configFilePath.c_str());

    wchar_t* parseEnd = nullptr;
    const float value = std::wcstof(buffer, &parseEnd);

    // Nothing parsed means the key is missing, empty, or not a number
    if (parseEnd == buffer) {
        return defaultValue;
    }

    return value;
}

FixConfig configLoad(const std::filesystem::path& configFilePath) {
    FixConfig config = DEFAULT_FIX_CONFIG;

    config.isEnabled = readBool(configFilePath, CONFIG_SECTION_GENERAL, CONFIG_KEY_ENABLED, config.isEnabled);

    config.forceRenderSize
        = readBool(configFilePath, CONFIG_SECTION_FIXES, CONFIG_KEY_FORCE_RENDER_SIZE, config.forceRenderSize);
    config.fitInside = readBool(configFilePath, CONFIG_SECTION_FIXES, CONFIG_KEY_FIT_INSIDE, config.fitInside);
    config.horPlus = readBool(configFilePath, CONFIG_SECTION_FIXES, CONFIG_KEY_HOR_PLUS, config.horPlus);
    config.textFix = readBool(configFilePath, CONFIG_SECTION_FIXES, CONFIG_KEY_TEXT_FIX, config.textFix);

    config.textOffsetX
        = readFloat(configFilePath, CONFIG_SECTION_OVERRIDES, CONFIG_KEY_TEXT_OFFSET_X, config.textOffsetX);
    config.textOffsetY
        = readFloat(configFilePath, CONFIG_SECTION_OVERRIDES, CONFIG_KEY_TEXT_OFFSET_Y, config.textOffsetY);

    return config;
}

#include "core/config.hpp"
#include "core/constants.hpp"
#include "core/hooks.hpp"
#include "core/log.hpp"
#include <filesystem>
#include <format>
#include <string>
#include <windows.h>

/**
 * @brief Returns the full path of a loaded module.
 * @param module The module, `nullptr` for the executable of the current process.
 * @return The full path of the module file.
 */
static std::filesystem::path getModulePath(HMODULE module) {
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));

    // A length equal to the buffer size means the path was truncated
    while (length == buffer.size()) {
        buffer.resize(buffer.size() * 2);
        length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
    }

    buffer.resize(length);
    return buffer;
}

/**
 * @brief Sets up the log, checks the host process, loads the configuration and installs the hooks.
 *
 * Runs synchronously from `DllMain`, Ultimate ASI Loader loads the plugins from the entry point of the game,
 * so the hooks are in place before the game creates its display data structures.
 * @param asiModule The module of this ASI.
 */
static void initialize(HMODULE asiModule) {
    const std::filesystem::path asiDirectory = getModulePath(asiModule).parent_path();

    logInit(asiDirectory / LOG_FILE_NAME);
    logWrite(std::format("{} v{}", FIX_NAME, FIX_VERSION));

    if (lstrcmpiW(getModulePath(nullptr).filename().c_str(), GAME_EXECUTABLE_NAME.data()) != 0) {
        logWrite("Not running inside the game executable, staying inactive");
        return;
    }

    const FixConfig config = configLoad(asiDirectory / CONFIG_FILE_NAME);

    logWrite(std::format("Config: enabled={} forceRenderSize={} fitInside={} horPlus={} textFix={} "
                         "centerFittedOffsets={} logFittedViews={}",
        config.isEnabled,
        config.forceRenderSize,
        config.fitInside,
        config.horPlus,
        config.textFix,
        config.centerFittedOffsets,
        config.logFittedViews));

    if (!config.isEnabled) {
        logWrite("Disabled in the configuration, staying inactive");
        return;
    }

    if (hooksInstall(GetModuleHandleW(nullptr), config)) {
        logWrite("Ready");
    } else {
        logWrite("Installation failed, see above");
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    // Note: No "DisableThreadLibraryCalls" here, the static CRT this DLL links against
    // relies on the thread attach/detach notifications to manage its per-thread data
    if (reason == DLL_PROCESS_ATTACH) {
        initialize(module);
    }

    return TRUE;
}

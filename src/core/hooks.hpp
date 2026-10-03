#pragma once

#include "types.hpp"
#include <windows.h>

/**
 * @brief Locates every hook site, then applies the patches and hooks enabled in the configuration.
 *
 * All-or-nothing: if any signature fails to match exactly once (e.g. after a game update),
 * nothing is patched and the game runs stock.
 * @param gameModule The game executable module.
 * @param config The fix configuration.
 * @return True if every enabled patch and hook was installed.
 */
bool hooksInstall(HMODULE gameModule, const FixConfig& config);

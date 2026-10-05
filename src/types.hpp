#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

/**
 * @brief A code signature used to locate a hook site inside the game executable.
 */
struct Signature {
    /**
     * @brief A short human-readable name, used in the log.
     */
    std::string_view name;

    /**
     * @brief The byte pattern to scan for, written as hex bytes separated by spaces,
     * with `??` as a wildcard (used for every RIP-relative displacement and call target).
     */
    std::string_view pattern;

    /**
     * @brief The offset from the start of the match to the hooked/patched address.
     */
    size_t hookOffset;
};

/**
 * @brief The fix configuration, read from the INI file next to the ASI.
 */
struct FixConfig {
    bool isEnabled; // Master switch, the ASI loads but patches nothing when false
    bool forceRenderSize; // Keeps the render target at the window size instead of the game's 16:9 fit
    bool fitInside; // Makes every view fit inside the screen instead of overflowing it
    bool horPlus; // Keeps the 16:9 vertical FOV and widens the horizontal one
    bool textFix; // Corrects the size of the text layer
    bool centerFittedOffsets; // Centers the 16:9 area every 2D layer (sprites and text) is drawn in
    bool logFittedViews; // Debug: logs every distinct view going through the viewport fit
};

/**
 * @brief Mirrors the layout of the game's screen size globals (four consecutive `int32_t`),
 * located through the operand of the render size setter.
 */
struct ScreenSizes {
    int32_t renderWidth; // Width of the render target (clamped to 16:9 by the game at startup)
    int32_t renderHeight; // Height of the render target
    int32_t windowWidth; // Width of the window, the actual output
    int32_t windowHeight; // Height of the window
};

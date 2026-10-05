#pragma once

#include "types.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>

/**
 * @brief The name of the fix, written at the top of the log.
 */
constexpr std::string_view FIX_NAME = "BGEWidescreenFix";

/**
 * @brief The version of the fix, written at the top of the log.
 */
constexpr std::string_view FIX_VERSION = "0.1.0";

/**
 * @brief The name of the game executable, the fix stays inactive inside any other process.
 */
constexpr std::wstring_view GAME_EXECUTABLE_NAME = L"bge.exe";

/**
 * @brief The name of the configuration file, located next to the ASI.
 */
constexpr std::wstring_view CONFIG_FILE_NAME = L"BGEWidescreenFix.ini";

/**
 * @brief The name of the log file, located next to the ASI.
 */
constexpr std::wstring_view LOG_FILE_NAME = L"BGEWidescreenFix.log";

/**
 * @brief The INI section holding the master switch.
 */
constexpr const wchar_t* CONFIG_SECTION_GENERAL = L"General";

/**
 * @brief The INI section holding the per-fix toggles.
 */
constexpr const wchar_t* CONFIG_SECTION_FIXES = L"Fixes";

/**
 * @brief The INI section holding the diagnostic toggles, all disabled by default.
 */
constexpr const wchar_t* CONFIG_SECTION_DEBUG = L"Debug";

/**
 * @brief The INI key of `FixConfig::isEnabled` (section `General`).
 */
constexpr const wchar_t* CONFIG_KEY_ENABLED = L"Enabled";

/**
 * @brief The INI key of `FixConfig::forceRenderSize` (section `Fixes`).
 */
constexpr const wchar_t* CONFIG_KEY_FORCE_RENDER_SIZE = L"ForceRenderSize";

/**
 * @brief The INI key of `FixConfig::fitInside` (section `Fixes`).
 */
constexpr const wchar_t* CONFIG_KEY_FIT_INSIDE = L"FitInside";

/**
 * @brief The INI key of `FixConfig::horPlus` (section `Fixes`).
 */
constexpr const wchar_t* CONFIG_KEY_HOR_PLUS = L"HorPlus";

/**
 * @brief The INI key of `FixConfig::textFix` (section `Fixes`).
 */
constexpr const wchar_t* CONFIG_KEY_TEXT_FIX = L"TextFix";

/**
 * @brief The INI key of `FixConfig::centerFittedOffsets` (section `Fixes`).
 */
constexpr const wchar_t* CONFIG_KEY_CENTER_FITTED_OFFSETS = L"CenterFittedOffsets";

/**
 * @brief The INI key of `FixConfig::logFittedViews` (section `Debug`).
 */
constexpr const wchar_t* CONFIG_KEY_LOG_FITTED_VIEWS = L"LogFittedViews";

/**
 * @brief The configuration used for every key missing from the INI file (or when there's no INI file at all).
 */
constexpr FixConfig DEFAULT_FIX_CONFIG {
    .isEnabled = true,
    .forceRenderSize = true,
    .fitInside = true,
    .horPlus = true,
    .textFix = true,
    .centerFittedOffsets = true,
    .logFittedViews = false,
};

/**
 * @brief The Y/X ratio of the 16:9 entry of the game's screen ratio table (`{ 1.0, 1.0, 0.75, 0.5625 }`).
 */
constexpr float STOCK_Y_OVER_X = 0.5625f;

/**
 * @brief The index of the 16:9 entry inside the screen ratio table, which is also the default value
 * of the `[Display] ScreenRatio` setting, only that entry is ever overridden.
 */
constexpr uintptr_t STOCK_RATIO_INDEX = 3;

/**
 * @brief The offset of the world camera inside a display data structure (Jade's `GDI_tdst_DisplayData`),
 * the viewport fit function receives it when fitting the 3D world view.
 */
constexpr uintptr_t WORLD_CAMERA_OFFSET = 0x1D0;

/**
 * @brief The offset of the size of a display data structure (two `int32_t`, width then height),
 * the size every camera of that structure is fitted into (the "virtual screen").
 */
constexpr uintptr_t DISPLAY_DATA_SIZE_OFFSET = 0x1A8;

/**
 * @brief The offset of the flags inside a camera (`uint32_t`, bit 0 = apply the screen ratio).
 */
constexpr uintptr_t CAMERA_FLAGS_OFFSET = 0x00;

/**
 * @brief The offset of the viewport size inside a camera (two `float`, width then height).
 */
constexpr uintptr_t CAMERA_VIEWPORT_SIZE_OFFSET = 0x24;

/**
 * @brief The offset of the viewport position and size fractions inside a camera (four `float`).
 */
constexpr uintptr_t CAMERA_VIEWPORT_FRACTIONS_OFFSET = 0x2C;

/**
 * @brief The offset of the fitted viewport offsets inside a camera (two `int32_t`, X then Y), written by the
 * viewport fit and added to every 2D position (sprites and text).
 */
constexpr uintptr_t CAMERA_FITTED_OFFSETS_OFFSET = 0x3C;

/**
 * @brief The maximum number of fitted view lines written to the log, so views whose properties keep
 * changing (e.g. animated) can't flood it.
 */
constexpr size_t MAX_FITTED_VIEW_LOG_LINES = 200;

/**
 * @brief The screen format flags the game writes when creating a display data structure.
 */
constexpr uint8_t STOCK_SCREEN_FORMAT_FLAGS = 0x08;

/**
 * @brief The screen format flags written instead, bit 0 makes the viewport fit inside the screen
 * instead of overflowing it.
 */
constexpr uint8_t FIT_INSIDE_SCREEN_FORMAT_FLAGS = 0x09;

/**
 * @brief The opcode the render size setter starts with, `cmp [rip + disp32], ecx` against the render
 * width global, the screen size globals (`ScreenSizes`) are resolved from its operand.
 */
constexpr uint8_t RENDER_SIZE_SETTER_OPCODE[2] = { 0x39, 0x0D };

/**
 * @brief The offset of the 32-bit displacement inside the first instruction of the render size setter.
 */
constexpr size_t RENDER_SIZE_SETTER_DISPLACEMENT_OFFSET = 2;

/**
 * @brief The length of the first instruction of the render size setter.
 */
constexpr size_t RENDER_SIZE_SETTER_INSTRUCTION_LENGTH = 6;

/**
 * @brief The offset of the 32-bit displacement inside a `call rel32` instruction.
 */
constexpr size_t CALL_REL32_DISPLACEMENT_OFFSET = 1;

/**
 * @brief The length of a `call rel32` instruction.
 */
constexpr size_t CALL_REL32_INSTRUCTION_LENGTH = 5;

/**
 * @brief Display data structure creation (`GDI_fnpst_CreateDisplayData`), hooked on the immediate
 * value of `mov dword ptr [rdi + 0x1B0], 8` (screen format flags).
 */
constexpr Signature SCREEN_FORMAT_FLAGS_SIGNATURE {
    .name = "Screen format flags",
    .pattern = "C7 87 B0 01 00 00 08 00 00 00 C7 87 B4 01 00 00 00 00 80 3F",
    .hookOffset = 0x06,
};

/**
 * @brief The `[Display] RenderWidth/RenderHeight` reading function, hooked on the call to the
 * render size setter (`void setter(int32_t width, int32_t height)`).
 */
constexpr Signature RENDER_SIZE_SETTER_SIGNATURE {
    .name = "Render size setter",
    .pattern = "8B D3 41 8B CF E8 ?? ?? ?? ?? 44 39 2D",
    .hookOffset = 0x05,
};

/**
 * @brief The viewport fit function, hooked right after the screen ratio has been loaded into `xmm6`
 * (`rdx` = ratio table index, `rbx` = display data, `r8` = fitted camera).
 */
constexpr Signature VIEWPORT_FIT_SIGNATURE {
    .name = "Viewport fit",
    .pattern = "48 63 93 CC 01 00 00 8D 42 FF 83 F8 02 77 0E 48 8D 0D ?? ?? ?? ?? F3 0F 10 34 91 EB 08 "
               "F3 0F 10 B3 B8 01 00 00 44 8B 9B B0 01 00 00",
    .hookOffset = 0x25,
};

/**
 * @brief The viewport fit, hooked right after the fitted offsets have been stored into the camera
 * (`mov [r8 + 0x3C], ecx` ... `mov [r8 + 0x40], eax`, `rbx` = display data, `r8` = fitted camera).
 */
constexpr Signature FITTED_OFFSETS_SIGNATURE {
    .name = "Fitted offsets",
    .pattern = "41 89 48 3C 48 8D 4D B0 F3 0F 10 1D ?? ?? ?? ?? 66 0F 6E C8 0F 5B C9 F3 0F 58 C8 F3 0F 2C C1 "
               "0F 10 4C 24 40 41 89 40 40 48 8B 05 ?? ?? ?? ??",
    .hookOffset = 0x28,
};

/**
 * @brief The per-frame camera update, hooked right before the display camera FOV is stored
 * (`movss [rdx + 0x1DC], xmm0`, `xmm0` = gameplay camera FOV * 1.06), drives the 3D world and every copy of the camera.
 */
constexpr Signature CAMERA_FOV_SIGNATURE {
    .name = "Camera FOV",
    .pattern = "F3 0F 10 83 A0 00 00 00 48 8B 15 ?? ?? ?? ?? F3 0F 59 05 ?? ?? ?? ?? 48 8D 8A D0 01 00 00 "
               "F3 0F 11 82 DC 01 00 00",
    .hookOffset = 0x1E,
};

/**
 * @brief The temporary FOV swap (save, swap, update, draw, restore), hooked right before the temporary FOV is stored
 * (`mov [rdx + 0x1DC], eax`, `eax` = the FOV bits), drives the camera-attached 3D HUD objects.
 */
constexpr Signature TEMPORARY_FOV_SIGNATURE {
    .name = "Temporary FOV",
    .pattern = "48 8B 15 ?? ?? ?? ?? 8B 47 08 F3 0F 10 B2 DC 01 00 00 48 8D 8A D0 01 00 00 89 82 DC 01 00 00 "
               "FF 52 40",
    .hookOffset = 0x19,
};

/**
 * @brief The text projection setup, hooked right before `mulss xmm2, [rcx + rax * 4]`
 * (`rax` = ratio table index, `xmm1` = FactorX, `xmm2` = `tan(FOV / 2)` about to be multiplied by the ratio).
 */
constexpr Signature TEXT_PROJECTION_SIGNATURE {
    .name = "Text projection",
    .pattern = "48 63 83 CC 01 00 00 48 8D 0D ?? ?? ?? ?? F3 0F 10 8B F4 01 00 00 0F 57 D2 F2 0F 5A D0 "
               "F3 0F 10 83 F8 01 00 00 F3 0F 59 CE F3 0F 59 C6 F3 0F 5E CA F3 0F 59 14 81",
    .hookOffset = 0x31,
};

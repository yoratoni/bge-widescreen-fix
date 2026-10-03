#include "core/hooks.hpp"
#include "core/constants.hpp"
#include "core/log.hpp"
#include "core/memory.hpp"
#include "utils/aspect.hpp"
#include <cstdint>
#include <cstring>
#include <format>
#include <optional>
#include <safetyhook.hpp>
#include <string_view>
#include <windows.h>

/**
 * @brief The game's render size setter, writes the render size globals and flags the change for the renderer.
 */
using RenderSizeSetter = void (*)(int32_t width, int32_t height);

static FixConfig fixConfig = DEFAULT_FIX_CONFIG;
static ScreenSizes* screenSizes = nullptr;
static RenderSizeSetter setRenderSize = nullptr;

/**
 * @brief Set by the viewport fit hook when the fitted view is the world camera, consumed by the projection
 * matrix hook (the viewport fit function calls the matrix builder on the same thread right after).
 */
static thread_local bool isWorldCameraFitted = false;

static safetyhook::MidHook viewportFitHook;
static safetyhook::MidHook projectionMatrixHook;
static safetyhook::MidHook textProjectionHook;
static safetyhook::MidHook textOriginHook;

/**
 * @brief Computes the aspect parameters of the output (the window).
 * @return The aspect parameters, or `std::nullopt` if the output isn't wider than 16:9.
 */
static std::optional<AspectParams> getOutputAspectParams() {
    return computeAspectParams(screenSizes->windowWidth, screenSizes->windowHeight);
}

/**
 * @brief Resizes the render target to the window size when they differ, through the game's own setter
 * (the game clamps the render target to 16:9 at startup, and may do it again on resolution changes).
 */
static void syncRenderSize() {
    static int32_t lastRequestedWidth { 0 };
    static int32_t lastRequestedHeight { 0 };

    const int32_t windowWidth = screenSizes->windowWidth;
    const int32_t windowHeight = screenSizes->windowHeight;

    if (!isWiderThan16By9(windowWidth, windowHeight)) {
        return;
    }

    if (screenSizes->renderWidth == windowWidth && screenSizes->renderHeight == windowHeight) {
        return;
    }

    // Only logged once per requested size, in case the game keeps reverting it
    if (windowWidth != lastRequestedWidth || windowHeight != lastRequestedHeight) {
        logWrite(std::format("Render size {}x{} -> {}x{}",
            screenSizes->renderWidth,
            screenSizes->renderHeight,
            windowWidth,
            windowHeight));

        lastRequestedWidth = windowWidth;
        lastRequestedHeight = windowHeight;
    }

    setRenderSize(windowWidth, windowHeight);
}

/**
 * @brief Viewport fit hook: replaces the stock 16:9 ratio with the output ratio for every view,
 * and flags the world camera for the Hor+ projection hook.
 * @param context The registers at the hook site.
 */
static void onViewportFit(safetyhook::Context& context) {
    isWorldCameraFitted = false;

    if (fixConfig.forceRenderSize) {
        syncRenderSize();
    }

    if (context.rdx != STOCK_RATIO_INDEX) {
        return;
    }

    const std::optional<AspectParams> aspectParams = getOutputAspectParams();
    if (!aspectParams) {
        return;
    }

    context.xmm6.f32[0] = aspectParams->yOverX;
    isWorldCameraFitted = context.r8 == context.rbx + WORLD_CAMERA_OFFSET;
}

/**
 * @brief Projection matrix hook: scales `tan(FOV / 2)` of the world camera so the vertical FOV stays
 * the 16:9 one and only the horizontal FOV widens (Hor+), every other view keeps the stock behaviour.
 * @param context The registers at the hook site.
 */
static void onProjectionMatrix(safetyhook::Context& context) {
    if (!isWorldCameraFitted) {
        return;
    }

    isWorldCameraFitted = false;

    const std::optional<AspectParams> aspectParams = getOutputAspectParams();
    if (!aspectParams) {
        return;
    }

    context.xmm0.f32[0] *= aspectParams->horPlusScale;
}

/**
 * @brief Text projection hook: corrects the size of the text layer.
 *
 * Measured in game: the text width follows `1 / FactorX` and the text height follows `FactorY / FactorX`,
 * scaling both factors by `horPlusScale` narrows the text back to its 16:9 proportions without changing its height.
 * @param context The registers at the hook site.
 */
static void onTextProjection(safetyhook::Context& context) {
    if (context.rax != STOCK_RATIO_INDEX) {
        return;
    }

    const std::optional<AspectParams> aspectParams = getOutputAspectParams();
    if (!aspectParams) {
        return;
    }

    // FactorX, already computed
    context.xmm1.f32[0] *= aspectParams->horPlusScale;

    // The hooked instruction multiplies by the stock 0.5625 right after, this makes the result
    // `tan(FOV / 2) * yOverX` instead, which is the FactorY divisor scaled by the same amount as FactorX
    context.xmm2.f32[0] *= aspectParams->textRatioScale;
}

/**
 * @brief Text origin hook, moves the text layer back in line with the rest of the 2D layers.
 * @param context The registers at the hook site.
 */
static void onTextOrigin(safetyhook::Context& context) {
    if (!getOutputAspectParams()) {
        return;
    }

    const float virtualWidth = context.xmm4.f32[0];
    const float virtualHeight = context.xmm3.f32[0];

    if (virtualWidth <= 0.0f || virtualHeight <= 0.0f) {
        return;
    }

    TextOffset textOffset = computeTextOffset(static_cast<float>(screenSizes->windowWidth),
        static_cast<float>(screenSizes->windowHeight),
        virtualWidth,
        virtualHeight);

    if (fixConfig.textOffsetX != 0.0f) {
        textOffset.x = fixConfig.textOffsetX;
    }

    if (fixConfig.textOffsetY != 0.0f) {
        textOffset.y = fixConfig.textOffsetY;
    }

    context.xmm0.f32[0] += textOffset.x;
    context.xmm2.f32[0] += textOffset.y;
}

/**
 * @brief Creates a mid-function hook and logs the result.
 * @param gameModule The game executable module (for the log).
 * @param hook The hook object to fill.
 * @param name The hook name (for the log).
 * @param address The hooked address.
 * @param callback The function called at the hook site.
 * @return True if the hook was created.
 */
static bool createMidHook(HMODULE gameModule,
    safetyhook::MidHook& hook,
    std::string_view name,
    uintptr_t address,
    safetyhook::MidHookFn callback) {
    hook = safetyhook::create_mid(reinterpret_cast<void*>(address), callback);

    if (!hook) {
        logWrite(std::format("[{}] Failed to hook {}", name, memoryFormatAddress(gameModule, address)));
        return false;
    }

    logWrite(std::format("[{}] Hooked", name));
    return true;
}

bool hooksInstall(HMODULE gameModule, const FixConfig& config) {
    fixConfig = config;

    // Locate and validate everything first, nothing gets patched unless every site was found
    const std::optional<uintptr_t> screenFormatFlagsAddress
        = memoryFindSignature(gameModule, SCREEN_FORMAT_FLAGS_SIGNATURE);
    const std::optional<uintptr_t> renderSizeSetterCallAddress
        = memoryFindSignature(gameModule, RENDER_SIZE_SETTER_SIGNATURE);
    const std::optional<uintptr_t> viewportFitAddress = memoryFindSignature(gameModule, VIEWPORT_FIT_SIGNATURE);
    const std::optional<uintptr_t> projectionMatrixAddress
        = memoryFindSignature(gameModule, PROJECTION_MATRIX_SIGNATURE);
    const std::optional<uintptr_t> textProjectionAddress = memoryFindSignature(gameModule, TEXT_PROJECTION_SIGNATURE);
    const std::optional<uintptr_t> textOriginAddress = memoryFindSignature(gameModule, TEXT_ORIGIN_SIGNATURE);

    if (!screenFormatFlagsAddress || !renderSizeSetterCallAddress || !viewportFitAddress || !projectionMatrixAddress
        || !textProjectionAddress || !textOriginAddress) {
        logWrite("At least one signature wasn't found, nothing was patched (unsupported game version?)");
        return false;
    }

    const uintptr_t renderSizeSetterAddress = memoryResolveRelative32(
        *renderSizeSetterCallAddress, CALL_REL32_DISPLACEMENT_OFFSET, CALL_REL32_INSTRUCTION_LENGTH);

    if (std::memcmp(reinterpret_cast<const void*>(renderSizeSetterAddress),
            RENDER_SIZE_SETTER_OPCODE,
            sizeof(RENDER_SIZE_SETTER_OPCODE))
        != 0) {
        logWrite("The render size setter doesn't start as expected, nothing was patched");
        return false;
    }

    const uint8_t currentScreenFormatFlags = *reinterpret_cast<const uint8_t*>(*screenFormatFlagsAddress);

    if (currentScreenFormatFlags != STOCK_SCREEN_FORMAT_FLAGS) {
        logWrite(std::format("Unexpected screen format flags 0x{:02X}, nothing was patched", currentScreenFormatFlags));
        return false;
    }

    setRenderSize = reinterpret_cast<RenderSizeSetter>(renderSizeSetterAddress);
    screenSizes = reinterpret_cast<ScreenSizes*>(memoryResolveRelative32(
        renderSizeSetterAddress, RENDER_SIZE_SETTER_DISPLACEMENT_OFFSET, RENDER_SIZE_SETTER_INSTRUCTION_LENGTH));

    logWrite(std::format("Render size setter at {}, screen sizes at {}",
        memoryFormatAddress(gameModule, renderSizeSetterAddress),
        memoryFormatAddress(gameModule, reinterpret_cast<uintptr_t>(screenSizes))));

    // Applied to every display data structure the game creates from now on
    if (fixConfig.fitInside) {
        const uint8_t fitInsideFlags[] = { FIT_INSIDE_SCREEN_FORMAT_FLAGS };

        if (!memoryWriteBytes(*screenFormatFlagsAddress, fitInsideFlags)) {
            logWrite("[Screen format flags] Failed to patch");
            return false;
        }

        logWrite("[Screen format flags] Patched (fit inside)");
    }

    if (!createMidHook(gameModule, viewportFitHook, VIEWPORT_FIT_SIGNATURE.name, *viewportFitAddress, onViewportFit)) {
        return false;
    }

    if (fixConfig.horPlus
        && !createMidHook(gameModule,
            projectionMatrixHook,
            PROJECTION_MATRIX_SIGNATURE.name,
            *projectionMatrixAddress,
            onProjectionMatrix)) {
        return false;
    }

    if (fixConfig.textFix) {
        if (!createMidHook(gameModule,
                textProjectionHook,
                TEXT_PROJECTION_SIGNATURE.name,
                *textProjectionAddress,
                onTextProjection)) {
            return false;
        }

        if (!createMidHook(gameModule, textOriginHook, TEXT_ORIGIN_SIGNATURE.name, *textOriginAddress, onTextOrigin)) {
            return false;
        }
    }

    return true;
}

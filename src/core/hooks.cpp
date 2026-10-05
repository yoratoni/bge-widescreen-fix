#include "core/hooks.hpp"
#include "core/constants.hpp"
#include "core/log.hpp"
#include "core/memory.hpp"
#include "utils/aspect.hpp"
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <format>
#include <mutex>
#include <optional>
#include <safetyhook.hpp>
#include <string>
#include <string_view>
#include <unordered_set>
#include <windows.h>

/**
 * @brief The game's render size setter, writes the render size globals and flags the change for the renderer.
 */
using RenderSizeSetter = void (*)(int32_t width, int32_t height);

static FixConfig fixConfig = DEFAULT_FIX_CONFIG;
static ScreenSizes* screenSizes = nullptr;
static RenderSizeSetter setRenderSize = nullptr;

/**
 * @brief Set by the viewport fit hook when the fitted view is the world camera and its ratio was replaced,
 * consumed by the fitted offsets hook (further down the same viewport fit call, on the same thread).
 */
static thread_local bool isWorldCameraFitted = false;

static safetyhook::MidHook viewportFitHook;
static safetyhook::MidHook fittedOffsetsHook;
static safetyhook::MidHook cameraFovHook;
static safetyhook::MidHook temporaryFovHook;
static safetyhook::MidHook textProjectionHook;

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
 * @brief Diagnostic: logs every distinct view going through the viewport fit, a view being described by
 * its camera's properties rather than its address (most cameras are rebuilt on the stack every frame).
 * The viewport fit may run on several threads, hence the mutex.
 * @param context The registers at the viewport fit hook (`rbx` = display data, `r8` = camera, `rdx` = ratio index).
 */
static void logFittedView(const safetyhook::Context& context) {
    static std::mutex fittedViewsMutex;
    static std::unordered_set<std::string> seenViews;
    static size_t loggedLines { 0 };

    const uintptr_t displayData = context.rbx;
    const uintptr_t camera = context.r8;

    const auto* displayDataSize = reinterpret_cast<const int32_t*>(displayData + DISPLAY_DATA_SIZE_OFFSET);
    const auto* cameraFlags = reinterpret_cast<const uint32_t*>(camera + CAMERA_FLAGS_OFFSET);
    const auto* viewportSize = reinterpret_cast<const float*>(camera + CAMERA_VIEWPORT_SIZE_OFFSET);
    const auto* viewportFractions = reinterpret_cast<const float*>(camera + CAMERA_VIEWPORT_FRACTIONS_OFFSET);

    std::string view = std::format("{} camera, flags 0x{:X}, ratio index {}, viewport {}x{}, fractions ({}, {}, {}, {}), "
                                   "display data size {}x{}",
        camera == displayData + WORLD_CAMERA_OFFSET ? "world" : "other",
        *cameraFlags,
        context.rdx,
        viewportSize[0],
        viewportSize[1],
        viewportFractions[0],
        viewportFractions[1],
        viewportFractions[2],
        viewportFractions[3],
        displayDataSize[0],
        displayDataSize[1]);

    std::lock_guard<std::mutex> lock(fittedViewsMutex);

    if (loggedLines >= MAX_FITTED_VIEW_LOG_LINES || !seenViews.insert(view).second) {
        return;
    }

    ++loggedLines;

    logWrite(std::format("[Fitted view] {} (camera 0x{:X}, render {}x{}, window {}x{})",
        view,
        camera,
        screenSizes->renderWidth,
        screenSizes->renderHeight,
        screenSizes->windowWidth,
        screenSizes->windowHeight));

    if (loggedLines == MAX_FITTED_VIEW_LOG_LINES) {
        logWrite("[Fitted view] Log limit reached, nothing else will be logged");
    }
}

/**
 * @brief Viewport fit hook: replaces the stock 16:9 ratio with the output ratio for every view,
 * and flags the world camera for the fitted offsets hook.
 * @param context The registers at the hook site.
 */
static void onViewportFit(safetyhook::Context& context) {
    isWorldCameraFitted = false;

    if (fixConfig.logFittedViews) {
        logFittedView(context);
    }

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
 * @brief Fitted offsets hook: every 2D position (sprites and text) gets the world camera's fitted offsets added to it,
 * fitted with the output ratio inside the (16:9) camera viewport they end up letterboxed and anchored to the left
 * edge, this centers the 16:9 area horizontally and removes the letterbox.
 * @param context The registers at the hook site.
 */
static void onFittedOffsets(safetyhook::Context& context) {
    // Only for the world camera, and only when the viewport fit hook replaced its ratio
    if (!isWorldCameraFitted) {
        return;
    }

    const auto* viewportSize = reinterpret_cast<const float*>(context.r8 + CAMERA_VIEWPORT_SIZE_OFFSET);

    if (viewportSize[0] <= 0.0f || viewportSize[1] <= 0.0f) {
        return;
    }

    const FittedOffsetCorrection correction = computeFittedOffsetCorrection(
        static_cast<float>(screenSizes->windowWidth),
        static_cast<float>(screenSizes->windowHeight),
        viewportSize[0],
        viewportSize[1]);

    // The fitted Y offset holds the letterbox the fit added, removing it moves the 2D layers back up
    auto* fittedOffsets = reinterpret_cast<int32_t*>(context.r8 + CAMERA_FITTED_OFFSETS_OFFSET);
    fittedOffsets[0] += static_cast<int32_t>(std::lround(correction.x));
    fittedOffsets[1] -= static_cast<int32_t>(std::lround(correction.y));
}

/**
 * @brief Camera FOV hook: widens the FOV the per-frame camera update writes into the display camera (Hor+),
 * the camera itself then has the wide FOV, so the 3D world, the water and every copy of the camera agree.
 * @param context The registers at the hook site (`xmm0` = the FOV about to be stored).
 */
static void onCameraFov(safetyhook::Context& context) {
    const std::optional<AspectParams> aspectParams = getOutputAspectParams();
    if (!aspectParams) {
        return;
    }

    context.xmm0.f32[0] = computeHorPlusFov(context.xmm0.f32[0], aspectParams->horPlusScale);
}

/**
 * @brief Temporary FOV hook: widens the FOV swapped in for a single draw (the camera-attached 3D HUD objects),
 * the swap restores the per-frame FOV afterwards, already widened by the camera FOV hook.
 * @param context The registers at the hook site (`eax` = the bits of the FOV about to be stored).
 */
static void onTemporaryFov(safetyhook::Context& context) {
    const std::optional<AspectParams> aspectParams = getOutputAspectParams();
    if (!aspectParams) {
        return;
    }

    const float fov = std::bit_cast<float>(static_cast<uint32_t>(context.rax));
    const float widenedFov = computeHorPlusFov(fov, aspectParams->horPlusScale);

    // Writing a 32-bit register zero-extends it on x64, the same as the original `eax` load did
    context.rax = std::bit_cast<uint32_t>(widenedFov);
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
    const std::optional<uintptr_t> cameraFovAddress = memoryFindSignature(gameModule, CAMERA_FOV_SIGNATURE);
    const std::optional<uintptr_t> temporaryFovAddress = memoryFindSignature(gameModule, TEMPORARY_FOV_SIGNATURE);
    const std::optional<uintptr_t> textProjectionAddress = memoryFindSignature(gameModule, TEXT_PROJECTION_SIGNATURE);
    const std::optional<uintptr_t> fittedOffsetsAddress = memoryFindSignature(gameModule, FITTED_OFFSETS_SIGNATURE);

    if (!screenFormatFlagsAddress || !renderSizeSetterCallAddress || !viewportFitAddress || !cameraFovAddress
        || !temporaryFovAddress || !textProjectionAddress || !fittedOffsetsAddress) {
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

    if (fixConfig.centerFittedOffsets
        && !createMidHook(
            gameModule, fittedOffsetsHook, FITTED_OFFSETS_SIGNATURE.name, *fittedOffsetsAddress, onFittedOffsets)) {
        return false;
    }

    if (fixConfig.horPlus) {
        if (!createMidHook(gameModule, cameraFovHook, CAMERA_FOV_SIGNATURE.name, *cameraFovAddress, onCameraFov)) {
            return false;
        }

        if (!createMidHook(
                gameModule, temporaryFovHook, TEMPORARY_FOV_SIGNATURE.name, *temporaryFovAddress, onTemporaryFov)) {
            return false;
        }
    }

    if (fixConfig.textFix
        && !createMidHook(gameModule,
            textProjectionHook,
            TEXT_PROJECTION_SIGNATURE.name,
            *textProjectionAddress,
            onTextProjection)) {
        return false;
    }

    return true;
}

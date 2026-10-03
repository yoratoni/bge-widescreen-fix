#pragma once

#include <cstdint>
#include <optional>

/**
 * @brief The values derived from the output resolution that the hooks write into the game.
 */
struct AspectParams {
    /**
     * @brief The Y/X ratio of the output (height / width), replaces the stock 16:9 ratio (0.5625).
     */
    float yOverX;

    /**
     * @brief The `tan(FOV / 2)` multiplier that keeps the 16:9 vertical FOV (Hor+), also the
     * text projection FactorX multiplier (`STOCK_Y_OVER_X / yOverX`).
     */
    float horPlusScale;

    /**
     * @brief The multiplier applied before the game's own `* 0.5625` so that the result
     * ends up multiplied by `yOverX` instead (`yOverX / STOCK_Y_OVER_X`).
     */
    float textRatioScale;
};

/**
 * @brief A pixel offset applied to the origin of the text layer.
 */
struct TextOffset {
    float x; // Horizontal offset in pixels, positive moves the text to the right
    float y; // Vertical offset in pixels, positive moves the text down
};

/**
 * @brief Checks whether a resolution is strictly wider than 16:9 (exact integer comparison).
 * @param width The width in pixels.
 * @param height The height in pixels.
 * @return True if the resolution is wider than 16:9, false otherwise (including invalid sizes).
 */
bool isWiderThan16By9(int32_t width, int32_t height);

/**
 * @brief Computes the aspect parameters for an output resolution.
 * @param width The output width in pixels.
 * @param height The output height in pixels.
 * @return The aspect parameters, or `std::nullopt` if the resolution isn't wider than 16:9
 * (the stock behaviour is kept in that case).
 */
std::optional<AspectParams> computeAspectParams(int32_t width, int32_t height);

/**
 * @brief Computes the offset that moves the text layer back in line with the rest of the 2D layers.
 *
 * The text layer is laid out in the game's virtual screen (the 16:9 size the render target had at startup),
 * fitted with the output Y/X ratio (letterboxed vertically) and anchored to the left edge, while every other
 * layer is centered on the output, the X offset re-centers the 16:9 area horizontally and the Y offset
 * removes the letterbox.
 * @param outputWidth The output width in pixels.
 * @param outputHeight The output height in pixels.
 * @param virtualWidth The virtual screen width in pixels.
 * @param virtualHeight The virtual screen height in pixels.
 * @return The text origin offset in pixels.
 */
TextOffset computeTextOffset(float outputWidth, float outputHeight, float virtualWidth, float virtualHeight);

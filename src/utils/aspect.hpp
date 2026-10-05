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
 * @brief The correction applied to the world camera's fitted offsets, in pixels.
 */
struct FittedOffsetCorrection {
    float x; // Horizontal correction in pixels, positive moves the 2D layers to the right
    float y; // Vertical correction in pixels, the letterbox the fit added (removed from the fitted Y offset)
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
 * @brief Widens a horizontal field of view for Hor+: the vertical field of view stays the 16:9 one,
 * only the horizontal one grows (`2 * atan(tan(fov / 2) * horPlusScale)`).
 * @param fov The horizontal field of view in radians, as set by the game for a 16:9 screen.
 * @param horPlusScale The Hor+ scale of the output (`AspectParams::horPlusScale`).
 * @return The widened horizontal field of view in radians.
 */
float computeHorPlusFov(float fov, float horPlusScale);

/**
 * @brief Computes the correction that centers the 16:9 area every 2D layer is drawn in.
 *
 * The 2D layers (sprites and text) are laid out in the camera viewport, a "virtual screen" sized once at startup
 * (16:9), and every 2D position gets the world camera's fitted offsets added to it. Fitted with the output Y/X
 * ratio inside that virtual screen, the offsets end up letterboxed vertically and anchored to the left edge,
 * the X correction re-centers the 16:9 area horizontally and the Y correction removes the letterbox.
 * @param outputWidth The output width in pixels.
 * @param outputHeight The output height in pixels.
 * @param virtualWidth The virtual screen width in pixels.
 * @param virtualHeight The virtual screen height in pixels.
 * @return The fitted offset correction in pixels.
 */
FittedOffsetCorrection computeFittedOffsetCorrection(
    float outputWidth, float outputHeight, float virtualWidth, float virtualHeight);

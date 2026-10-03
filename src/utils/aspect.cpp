#include "utils/aspect.hpp"
#include "core/constants.hpp"
#include <cstdint>
#include <optional>

bool isWiderThan16By9(int32_t width, int32_t height) {
    if (width <= 0 || height <= 0) {
        return false;
    }

    // Cross-multiplied in 64 bits to avoid both rounding and overflow (width / height > 16 / 9)
    return static_cast<int64_t>(width) * 9 > static_cast<int64_t>(height) * 16;
}

std::optional<AspectParams> computeAspectParams(int32_t width, int32_t height) {
    if (!isWiderThan16By9(width, height)) {
        return std::nullopt;
    }

    const float yOverX = static_cast<float>(height) / static_cast<float>(width);

    return AspectParams {
        .yOverX = yOverX,
        .horPlusScale = STOCK_Y_OVER_X / yOverX,
        .textRatioScale = yOverX / STOCK_Y_OVER_X,
    };
}

TextOffset computeTextOffset(float outputWidth, float outputHeight, float virtualWidth, float virtualHeight) {
    // The virtual screen scaled to the output height gives the width of the centered 16:9 area,
    // the text layer is drawn from the left edge of the output instead of the left edge of that area
    const float scaledVirtualWidth = virtualWidth * outputHeight / virtualHeight;

    // The viewport fit letterboxes the virtual screen with the output Y/X ratio,
    // the text layer is drawn that many pixels too high
    const float fittedVirtualHeight = virtualWidth * outputHeight / outputWidth;

    return TextOffset {
        .x = (outputWidth - scaledVirtualWidth) / 2.0f,
        .y = (virtualHeight - fittedVirtualHeight) / 2.0f,
    };
}

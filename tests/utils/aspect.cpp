#include "utils/aspect.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE("isWiderThan16By9", "[utils][aspect]") {
    SECTION("ultrawide resolutions") {
        CHECK(isWiderThan16By9(3440, 1440));
        CHECK(isWiderThan16By9(2560, 1080));
        CHECK(isWiderThan16By9(5120, 1440));
    }

    SECTION("exactly 16:9 is not wider") {
        CHECK_FALSE(isWiderThan16By9(2560, 1440));
        CHECK_FALSE(isWiderThan16By9(1920, 1080));
    }

    SECTION("narrower than 16:9") {
        CHECK_FALSE(isWiderThan16By9(1920, 1200));
        CHECK_FALSE(isWiderThan16By9(1024, 768));
    }

    SECTION("invalid sizes") {
        CHECK_FALSE(isWiderThan16By9(0, 0));
        CHECK_FALSE(isWiderThan16By9(3440, 0));
        CHECK_FALSE(isWiderThan16By9(-3440, 1440));
    }
}

TEST_CASE("computeAspectParams", "[utils][aspect]") {
    SECTION("3440x1440") {
        const auto aspectParams = computeAspectParams(3440, 1440);
        REQUIRE(aspectParams.has_value());

        // Y/X: 1440 / 3440 = 0.418605
        // Hor+ scale: 0.5625 / 0.418605 = 1.34375 (= 3440 / 2560)
        // Text ratio scale: 0.418605 / 0.5625 = 0.744186 (= 2560 / 3440)
        CHECK_THAT(aspectParams->yOverX, WithinAbs(0.418605, 1e-5));
        CHECK_THAT(aspectParams->horPlusScale, WithinAbs(1.34375, 1e-5));
        CHECK_THAT(aspectParams->textRatioScale, WithinAbs(0.744186, 1e-5));
    }

    SECTION("Hor+ scale and text ratio scale are inverses") {
        const auto aspectParams = computeAspectParams(2560, 1080);
        REQUIRE(aspectParams.has_value());

        CHECK_THAT(aspectParams->horPlusScale * aspectParams->textRatioScale, WithinAbs(1.0, 1e-5));
    }

    SECTION("16:9 and narrower keep the stock behaviour") {
        CHECK_FALSE(computeAspectParams(2560, 1440).has_value());
        CHECK_FALSE(computeAspectParams(1920, 1200).has_value());
    }
}

TEST_CASE("computeTextOffset", "[utils][aspect]") {
    SECTION("3440x1440 output, 2560x1440 virtual screen (values tuned in game)") {
        const TextOffset textOffset = computeTextOffset(3440.0f, 1440.0f, 2560.0f, 1440.0f);

        // X: (3440 - 2560) / 2 = 440
        // Y: (1440 - 2560 * 1440 / 3440) / 2 = (1440 - 1071.63) / 2 = 184.19
        CHECK_THAT(textOffset.x, WithinAbs(440.0, 1e-3));
        CHECK_THAT(textOffset.y, WithinAbs(184.186, 1e-3));
    }

    SECTION("2560x1080 output, 1920x1080 virtual screen") {
        const TextOffset textOffset = computeTextOffset(2560.0f, 1080.0f, 1920.0f, 1080.0f);

        // X: (2560 - 1920) / 2 = 320
        // Y: (1080 - 1920 * 1080 / 2560) / 2 = (1080 - 810) / 2 = 135
        CHECK_THAT(textOffset.x, WithinAbs(320.0, 1e-3));
        CHECK_THAT(textOffset.y, WithinAbs(135.0, 1e-3));
    }

    SECTION("a 16:9 output needs no offset") {
        const TextOffset textOffset = computeTextOffset(2560.0f, 1440.0f, 2560.0f, 1440.0f);

        CHECK_THAT(textOffset.x, WithinAbs(0.0, 1e-3));
        CHECK_THAT(textOffset.y, WithinAbs(0.0, 1e-3));
    }
}

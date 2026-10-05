#include "utils/aspect.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

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

TEST_CASE("computeHorPlusFov", "[utils][aspect]") {
    SECTION("3440x1440, gameplay camera FOV measured in game") {
        // tan(1.06 / 2) = 0.58592, * 1.34375 = 0.78733, 2 * atan(0.78733) = 1.33393
        CHECK_THAT(computeHorPlusFov(1.06f, 1.34375f), WithinAbs(1.33393, 1e-4));
    }

    SECTION("the vertical field of view stays the 16:9 one") {
        const auto aspectParams = computeAspectParams(3440, 1440);
        REQUIRE(aspectParams.has_value());

        const float fov = 1.06f;
        const float widenedFov = computeHorPlusFov(fov, aspectParams->horPlusScale);

        // Vertical half-extent = horizontal half-extent * Y/X, at 16:9 and at the output ratio
        const float stockVertical = std::tan(fov / 2.0f) * 0.5625f;
        const float widenedVertical = std::tan(widenedFov / 2.0f) * aspectParams->yOverX;

        CHECK_THAT(widenedVertical, WithinAbs(stockVertical, 1e-5));
    }

    SECTION("a scale of 1 keeps the field of view") {
        CHECK_THAT(computeHorPlusFov(1.06f, 1.0f), WithinAbs(1.06, 1e-5));
    }
}

TEST_CASE("computeFittedOffsetCorrection", "[utils][aspect]") {
    SECTION("3440x1440 output, 2560x1440 virtual screen (values tuned in game)") {
        const FittedOffsetCorrection correction = computeFittedOffsetCorrection(3440.0f, 1440.0f, 2560.0f, 1440.0f);

        // X: (3440 - 2560) / 2 = 440
        // Y: (1440 - 2560 * 1440 / 3440) / 2 = (1440 - 1071.63) / 2 = 184.19
        CHECK_THAT(correction.x, WithinAbs(440.0, 1e-3));
        CHECK_THAT(correction.y, WithinAbs(184.186, 1e-3));
    }

    SECTION("2560x1080 output, 1920x1080 virtual screen") {
        const FittedOffsetCorrection correction = computeFittedOffsetCorrection(2560.0f, 1080.0f, 1920.0f, 1080.0f);

        // X: (2560 - 1920) / 2 = 320
        // Y: (1080 - 1920 * 1080 / 2560) / 2 = (1080 - 810) / 2 = 135
        CHECK_THAT(correction.x, WithinAbs(320.0, 1e-3));
        CHECK_THAT(correction.y, WithinAbs(135.0, 1e-3));
    }

    SECTION("a 16:9 output needs no correction") {
        const FittedOffsetCorrection correction = computeFittedOffsetCorrection(2560.0f, 1440.0f, 2560.0f, 1440.0f);

        CHECK_THAT(correction.x, WithinAbs(0.0, 1e-3));
        CHECK_THAT(correction.y, WithinAbs(0.0, 1e-3));
    }
}

#include "core/constants.hpp"
#include "utils/pattern.hpp"
#include <catch2/catch_test_macros.hpp>
#include <optional>

// Every signature was checked to match exactly once in bge.exe (Aug 2 2024 build),
// these tests only guard against typos when editing them
TEST_CASE("Signatures parse and hook a concrete byte inside the pattern", "[core][constants]") {
    const Signature signatures[] = {
        SCREEN_FORMAT_FLAGS_SIGNATURE,
        RENDER_SIZE_SETTER_SIGNATURE,
        VIEWPORT_FIT_SIGNATURE,
        PROJECTION_MATRIX_SIGNATURE,
        TEXT_PROJECTION_SIGNATURE,
        TEXT_ORIGIN_SIGNATURE,
    };

    for (const Signature& signature : signatures) {
        INFO(signature.name);

        const std::optional<BytePattern> pattern = parseBytePattern(signature.pattern);
        REQUIRE(pattern.has_value());

        // Hooks and patches must land on known code, never on a wildcard (RIP-relative displacement)
        REQUIRE(signature.hookOffset < pattern->size());
        CHECK((*pattern)[signature.hookOffset].has_value());
    }
}

TEST_CASE("Patched sites point at the expected bytes", "[core][constants]") {
    SECTION("the screen format flags immediate is the stock value") {
        const std::optional<BytePattern> pattern = parseBytePattern(SCREEN_FORMAT_FLAGS_SIGNATURE.pattern);
        REQUIRE(pattern.has_value());

        CHECK((*pattern)[SCREEN_FORMAT_FLAGS_SIGNATURE.hookOffset] == STOCK_SCREEN_FORMAT_FLAGS);
    }

    SECTION("the render size setter is reached through a call rel32") {
        const std::optional<BytePattern> pattern = parseBytePattern(RENDER_SIZE_SETTER_SIGNATURE.pattern);
        REQUIRE(pattern.has_value());

        // 0xE8 = call rel32
        CHECK((*pattern)[RENDER_SIZE_SETTER_SIGNATURE.hookOffset] == 0xE8);
    }
}

#include "utils/pattern.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

TEST_CASE("parseBytePattern", "[utils][pattern]") {
    SECTION("concrete bytes") {
        const auto pattern = parseBytePattern("48 8D 0D");
        REQUIRE(pattern.has_value());
        CHECK(*pattern == BytePattern { 0x48, 0x8D, 0x0D });
    }

    SECTION("wildcards (single and double question marks)") {
        const auto pattern = parseBytePattern("E8 ?? ? 90");
        REQUIRE(pattern.has_value());
        CHECK(*pattern == BytePattern { 0xE8, std::nullopt, std::nullopt, 0x90 });
    }

    SECTION("lowercase hex and extra whitespace") {
        const auto pattern = parseBytePattern("  f3 0f\t10  ");
        REQUIRE(pattern.has_value());
        CHECK(*pattern == BytePattern { 0xF3, 0x0F, 0x10 });
    }

    SECTION("invalid tokens are rejected") {
        CHECK_FALSE(parseBytePattern("4").has_value());
        CHECK_FALSE(parseBytePattern("488D").has_value());
        CHECK_FALSE(parseBytePattern("0x48").has_value());
        CHECK_FALSE(parseBytePattern("GG").has_value());
        CHECK_FALSE(parseBytePattern("48 ??? 8D").has_value());
    }

    SECTION("empty pattern is rejected") {
        CHECK_FALSE(parseBytePattern("").has_value());
        CHECK_FALSE(parseBytePattern("   ").has_value());
    }
}

TEST_CASE("findBytePattern", "[utils][pattern]") {
    const std::vector<uint8_t> data = { 0x90, 0x48, 0x8D, 0x0D, 0x11, 0x22, 0x48, 0x8D, 0x0D, 0x33, 0x90 };

    SECTION("single match") {
        const auto matches = findBytePattern(data, *parseBytePattern("0D 11"));
        CHECK(matches == std::vector<size_t> { 3 });
    }

    SECTION("multiple matches in ascending order") {
        const auto matches = findBytePattern(data, *parseBytePattern("48 8D 0D"));
        CHECK(matches == std::vector<size_t> { 1, 6 });
    }

    SECTION("wildcards match any byte") {
        const auto matches = findBytePattern(data, *parseBytePattern("48 8D 0D ?? 22"));
        CHECK(matches == std::vector<size_t> { 1 });
    }

    SECTION("leading wildcard (anchor isn't the first byte)") {
        const auto matches = findBytePattern(data, *parseBytePattern("?? 48 8D"));
        CHECK(matches == std::vector<size_t> { 0, 5 });
    }

    SECTION("match at the very end") {
        const auto matches = findBytePattern(data, *parseBytePattern("33 90"));
        CHECK(matches == std::vector<size_t> { 9 });
    }

    SECTION("no match") {
        CHECK(findBytePattern(data, *parseBytePattern("48 8D 0E")).empty());
    }

    SECTION("pattern longer than the data") {
        const std::vector<uint8_t> shortData = { 0x48, 0x8D };
        CHECK(findBytePattern(shortData, *parseBytePattern("48 8D 0D")).empty());
    }
}

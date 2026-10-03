#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

/**
 * @brief A parsed byte pattern, `std::nullopt` entries are wildcards matching any byte.
 */
using BytePattern = std::vector<std::optional<uint8_t>>;

/**
 * @brief Parses a byte pattern written as hex bytes separated by whitespace (e.g. `"48 8D 0D ?? ?? ?? ??"`).
 * @param patternString The pattern string, `??` or `?` being a wildcard.
 * @return The parsed pattern, or `std::nullopt` if the string is empty or contains an invalid token.
 */
std::optional<BytePattern> parseBytePattern(std::string_view patternString);

/**
 * @brief Finds every occurrence of a byte pattern inside a byte span.
 * @param data The bytes to scan.
 * @param pattern The pattern to look for.
 * @return The offsets of every match from the start of `data`, in ascending order.
 */
std::vector<size_t> findBytePattern(std::span<const uint8_t> data, const BytePattern& pattern);

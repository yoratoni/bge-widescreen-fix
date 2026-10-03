#include "utils/pattern.hpp"
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

std::optional<BytePattern> parseBytePattern(std::string_view patternString) {
    BytePattern pattern;

    size_t position { 0 };

    while (position < patternString.size()) {
        // Skip the whitespace separating tokens
        if (patternString[position] == ' ' || patternString[position] == '\t') {
            ++position;
            continue;
        }

        size_t tokenEnd = patternString.find_first_of(" \t", position);
        if (tokenEnd == std::string_view::npos) {
            tokenEnd = patternString.size();
        }

        const std::string_view token = patternString.substr(position, tokenEnd - position);
        position = tokenEnd;

        if (token == "??" || token == "?") {
            pattern.push_back(std::nullopt);
            continue;
        }

        // Every concrete token must be exactly two hex digits
        uint8_t byteValue { 0 };
        const auto [parseEnd, parseError] = std::from_chars(token.data(), token.data() + token.size(), byteValue, 16);

        if (token.size() != 2 || parseError != std::errc {} || parseEnd != token.data() + token.size()) {
            return std::nullopt;
        }

        pattern.push_back(byteValue);
    }

    if (pattern.empty()) {
        return std::nullopt;
    }

    return pattern;
}

std::vector<size_t> findBytePattern(std::span<const uint8_t> data, const BytePattern& pattern) {
    std::vector<size_t> matches;

    if (pattern.empty() || pattern.size() > data.size()) {
        return matches;
    }

    // The first concrete byte is used as an anchor so most positions are skipped with "memchr",
    // a pattern made only of wildcards matches at every position
    size_t anchorIndex { 0 };
    while (anchorIndex < pattern.size() && !pattern[anchorIndex].has_value()) {
        ++anchorIndex;
    }

    const size_t lastStart = data.size() - pattern.size();

    for (size_t start = 0; start <= lastStart; ++start) {
        if (anchorIndex < pattern.size()) {
            const uint8_t* searchBegin = data.data() + start + anchorIndex;
            const size_t searchLength = lastStart - start + 1;

            const void* anchor = std::memchr(searchBegin, *pattern[anchorIndex], searchLength);
            if (!anchor) {
                break;
            }

            start = static_cast<size_t>(static_cast<const uint8_t*>(anchor) - data.data()) - anchorIndex;
        }

        bool isMatch = true;

        for (size_t i = 0; i < pattern.size(); ++i) {
            if (pattern[i].has_value() && data[start + i] != *pattern[i]) {
                isMatch = false;
                break;
            }
        }

        if (isMatch) {
            matches.push_back(start);
        }
    }

    return matches;
}

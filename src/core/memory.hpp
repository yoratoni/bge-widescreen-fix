#pragma once

#include "types.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <windows.h>

/**
 * @brief Returns the executable sections of a loaded module that aren't writable.
 *
 * Denuvo's own virtual machine lives in a huge writable + executable section, skipping writable
 * sections keeps the scans on the game's actual code (and avoids scanning ~330 MB for nothing).
 * @param module The loaded module.
 * @return The byte spans of the matching sections.
 */
std::vector<std::span<const uint8_t>> memoryGetCodeSections(HMODULE module);

/**
 * @brief Finds the hook address of a signature, the signature must match exactly once
 * across all code sections (logs the result either way).
 * @param module The loaded module to scan.
 * @param signature The signature to look for.
 * @return The matched address plus `signature.hookOffset`, or `std::nullopt` on zero/multiple matches.
 */
std::optional<uintptr_t> memoryFindSignature(HMODULE module, const Signature& signature);

/**
 * @brief Resolves the target of a RIP-relative operand (`call rel32`, `mov [rip + disp32]`, ...).
 * @param instructionAddress The address of the instruction.
 * @param displacementOffset The offset of the 32-bit displacement inside the instruction.
 * @param instructionLength The total length of the instruction (RIP points right after it).
 * @return The absolute target address.
 */
uintptr_t memoryResolveRelative32(uintptr_t instructionAddress, size_t displacementOffset, size_t instructionLength);

/**
 * @brief Overwrites bytes in a (possibly read-only) code page and flushes the instruction cache.
 * @param address The address to write to.
 * @param bytes The bytes to write.
 * @return True if the bytes were written.
 */
bool memoryWriteBytes(uintptr_t address, std::span<const uint8_t> bytes);

/**
 * @brief Formats an address as an offset from the module base (`bge.exe+0x...`), for the log.
 * @param module The module the address belongs to.
 * @param address The address to format.
 * @return The formatted address.
 */
std::string memoryFormatAddress(HMODULE module, uintptr_t address);

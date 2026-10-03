#include "core/memory.hpp"
#include "core/log.hpp"
#include "utils/pattern.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <windows.h>

std::vector<std::span<const uint8_t>> memoryGetCodeSections(HMODULE module) {
    std::vector<std::span<const uint8_t>> sections;

    // Integer arithmetic on the base keeps the casts to the PE header structs alignment-neutral
    const uintptr_t moduleBase = reinterpret_cast<uintptr_t>(module);
    const auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(moduleBase);
    const auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS*>(moduleBase + dosHeader->e_lfanew);

    const IMAGE_SECTION_HEADER* sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);

    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i, ++sectionHeader) {
        const DWORD characteristics = sectionHeader->Characteristics;

        const bool isExecutable = (characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
        const bool isWritable = (characteristics & IMAGE_SCN_MEM_WRITE) != 0;

        if (!isExecutable || isWritable) {
            continue;
        }

        const auto* sectionStart = reinterpret_cast<const uint8_t*>(moduleBase + sectionHeader->VirtualAddress);
        sections.emplace_back(sectionStart, sectionHeader->Misc.VirtualSize);
    }

    return sections;
}

std::optional<uintptr_t> memoryFindSignature(HMODULE module, const Signature& signature) {
    const std::optional<BytePattern> pattern = parseBytePattern(signature.pattern);

    if (!pattern) {
        logWrite(std::format("[{}] Invalid pattern", signature.name));
        return std::nullopt;
    }

    std::vector<uintptr_t> matches;

    for (const std::span<const uint8_t>& section : memoryGetCodeSections(module)) {
        for (const size_t offset : findBytePattern(section, *pattern)) {
            matches.push_back(reinterpret_cast<uintptr_t>(section.data()) + offset);
        }
    }

    if (matches.size() != 1) {
        logWrite(std::format("[{}] Expected exactly 1 match, found {}", signature.name, matches.size()));
        return std::nullopt;
    }

    const uintptr_t hookAddress = matches.front() + signature.hookOffset;
    logWrite(std::format("[{}] Found at {}", signature.name, memoryFormatAddress(module, hookAddress)));

    return hookAddress;
}

uintptr_t memoryResolveRelative32(uintptr_t instructionAddress, size_t displacementOffset, size_t instructionLength) {
    int32_t displacement { 0 };
    std::memcpy(&displacement, reinterpret_cast<const void*>(instructionAddress + displacementOffset), sizeof(displacement));

    return instructionAddress + instructionLength + static_cast<uintptr_t>(static_cast<intptr_t>(displacement));
}

bool memoryWriteBytes(uintptr_t address, std::span<const uint8_t> bytes) {
    void* target = reinterpret_cast<void*>(address);

    DWORD oldProtection { 0 };
    if (!VirtualProtect(target, bytes.size(), PAGE_EXECUTE_READWRITE, &oldProtection)) {
        return false;
    }

    std::memcpy(target, bytes.data(), bytes.size());

    DWORD unusedProtection { 0 };
    VirtualProtect(target, bytes.size(), oldProtection, &unusedProtection);
    FlushInstructionCache(GetCurrentProcess(), target, bytes.size());

    return true;
}

std::string memoryFormatAddress(HMODULE module, uintptr_t address) {
    return std::format("bge.exe+0x{:X}", address - reinterpret_cast<uintptr_t>(module));
}

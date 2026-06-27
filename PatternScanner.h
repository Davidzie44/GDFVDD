#pragma once

#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>

struct ModuleInfo {
    uintptr_t baseAddress;
    size_t size;
    std::wstring name;
};

class PatternScanner {
private:
    HANDLE processHandle;

    struct SectionHeader {
        uint32_t virtualAddress;
        uint32_t sizeOfRawData;
        uint32_t pointerToRawData;
        std::string name;
    };

    std::vector<SectionHeader> ParseSectionHeaders(uintptr_t moduleBase) {
        std::vector<SectionHeader> sections;

        // Read DOS header
        IMAGE_DOS_HEADER dosHeader;
        if (!ReadProcessMemory(processHandle, reinterpret_cast<LPCVOID>(moduleBase), 
                              &dosHeader, sizeof(dosHeader), nullptr)) {
            throw std::runtime_error("Failed to read DOS header");
        }

        if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE) {
            throw std::runtime_error("Invalid DOS signature");
        }

        // Read NT headers
        IMAGE_NT_HEADERS ntHeaders;
        uintptr_t ntHeadersOffset = moduleBase + dosHeader.e_lfanew;
        if (!ReadProcessMemory(processHandle, reinterpret_cast<LPCVOID>(ntHeadersOffset),
                              &ntHeaders, sizeof(ntHeaders), nullptr)) {
            throw std::runtime_error("Failed to read NT headers");
        }

        if (ntHeaders.Signature != IMAGE_NT_SIGNATURE) {
            throw std::runtime_error("Invalid NT signature");
        }

        // Read section headers
        uintptr_t sectionHeaderOffset = ntHeadersOffset + offsetof(IMAGE_NT_HEADERS, OptionalHeader) +
                                        ntHeaders.FileHeader.SizeOfOptionalHeader;
        
        for (int i = 0; i < ntHeaders.FileHeader.NumberOfSections; i++) {
            IMAGE_SECTION_HEADER sectionHeader;
            if (!ReadProcessMemory(processHandle, reinterpret_cast<LPCVOID>(sectionHeaderOffset + i * sizeof(IMAGE_SECTION_HEADER)),
                                  &sectionHeader, sizeof(sectionHeader), nullptr)) {
                continue;
            }

            SectionHeader header;
            header.name = std::string(reinterpret_cast<const char*>(sectionHeader.Name), 8);
            header.virtualAddress = sectionHeader.VirtualAddress;
            header.sizeOfRawData = sectionHeader.SizeOfRawData;
            header.pointerToRawData = sectionHeader.PointerToRawData;
            sections.push_back(header);
        }

        return sections;
    }

public:
    PatternScanner(HANDLE handle) : processHandle(handle) {
        if (handle == INVALID_HANDLE_VALUE || handle == NULL) {
            throw std::runtime_error("Invalid process handle");
        }
    }

    ModuleInfo GetModuleInfo(const std::wstring& moduleName) {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, 
                                                  GetProcessId(processHandle));
        if (snapshot == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Failed to create module snapshot");
        }

        MODULEENTRY32W entry;
        entry.dwSize = sizeof(MODULEENTRY32W);

        if (!Module32FirstW(snapshot, &entry)) {
            CloseHandle(snapshot);
            throw std::runtime_error("Failed to retrieve first module");
        }

        ModuleInfo info = {0, 0, L""};
        do {
            if (moduleName == entry.szModule) {
                info.baseAddress = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
                info.size = entry.modBaseSize;
                info.name = entry.szModule;
                break;
            }
        } while (Module32NextW(snapshot, &entry));

        CloseHandle(snapshot);

        if (info.baseAddress == 0) {
            throw std::runtime_error("Module not found: " + std::string(moduleName.begin(), moduleName.end()));
        }

        return info;
    }

    // Pattern format: "48 8B ? ? ? 00" where ? is wildcard (skip byte), 00 is literal 0x00
    uintptr_t ScanPattern(uintptr_t moduleBase, const std::string& pattern) {
        return ScanPattern(moduleBase, 0x10000000, pattern); // Default to large scan size
    }

    uintptr_t ScanPattern(uintptr_t moduleBase, size_t scanSize, const std::string& pattern) {
        // Parse pattern into bytes and wildcards
        std::vector<uint8_t> patternBytes;
        std::vector<bool> wildcards;

        std::string::size_type pos = 0;
        while (pos < pattern.length()) {
            // Skip whitespace
            while (pos < pattern.length() && pattern[pos] == ' ') {
                pos++;
            }
            if (pos >= pattern.length()) break;

            if (pattern[pos] == '?') {
                patternBytes.push_back(0);
                wildcards.push_back(true);
                pos++;
            } else {
                if (pos + 1 >= pattern.length()) {
                    throw std::runtime_error("Invalid pattern format");
                }
                
                std::string byteStr = pattern.substr(pos, 2);
                uint8_t byte = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
                patternBytes.push_back(byte);
                wildcards.push_back(false);
                pos += 2;
            }
        }

        if (patternBytes.empty()) {
            throw std::runtime_error("Empty pattern");
        }

        // Read module memory
        std::vector<uint8_t> memory(scanSize);
        SIZE_T bytesRead;
        if (!ReadProcessMemory(processHandle, reinterpret_cast<LPCVOID>(moduleBase),
                              memory.data(), scanSize, &bytesRead)) {
            throw std::runtime_error("Failed to read module memory");
        }

        // Scan for pattern
        size_t patternSize = patternBytes.size();
        for (size_t i = 0; i <= bytesRead - patternSize; i++) {
            bool match = true;
            for (size_t j = 0; j < patternSize; j++) {
                if (!wildcards[j] && memory[i + j] != patternBytes[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return moduleBase + i;
            }
        }

        return 0; // Pattern not found
    }

    uintptr_t ScanTextSection(uintptr_t moduleBase, const std::string& pattern) {
        auto sections = ParseSectionHeaders(moduleBase);

        for (const auto& section : sections) {
            // Find .text section (case-insensitive)
            std::string sectionNameLower = section.name;
            for (auto& c : sectionNameLower) c = tolower(c);
            
            if (sectionNameLower.find(".text") != std::string::npos) {
                uintptr_t sectionBase = moduleBase + section.virtualAddress;
                return ScanPattern(sectionBase, section.sizeOfRawData, pattern);
            }
        }

        throw std::runtime_error(".text section not found");
    }

    // Scan with multiple patterns, returns first match
    uintptr_t ScanPatterns(uintptr_t moduleBase, const std::vector<std::string>& patterns) {
        for (const auto& pattern : patterns) {
            uintptr_t result = ScanPattern(moduleBase, pattern);
            if (result != 0) {
                return result;
            }
        }
        return 0;
    }

    // Add offset to result (for RIP-relative addressing)
    static uintptr_t AddOffset(uintptr_t address, int32_t offset) {
        if (address == 0) return 0;
        return address + offset;
    }

    // Read 32-bit relative offset and calculate absolute address
    static uintptr_t ResolveRelativeAddress(uintptr_t address, int32_t instructionLength = 4) {
        if (address == 0) return 0;
        
        int32_t relativeOffset;
        memcpy(&relativeOffset, reinterpret_cast<void*>(address + instructionLength - 4), sizeof(relativeOffset));
        
        return address + instructionLength + relativeOffset;
    }
};

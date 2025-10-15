#include "utils.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>
#include <climits>

namespace HotstateSim {

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

bool startsWith(const std::string& str, const std::string& prefix) {
    return str.length() >= prefix.length() && 
           str.substr(0, prefix.length()) == prefix;
}

bool endsWith(const std::string& str, const std::string& suffix) {
    return str.length() >= suffix.length() && 
           str.substr(str.length() - suffix.length()) == suffix;
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(trim(token));
    }
    return tokens;
}

uint32_t parseHex(const std::string& hexStr) {
    std::string trimmed = trim(hexStr);
    // Remove 0x prefix if present
    if (startsWith(trimmed, "0x") || startsWith(trimmed, "0X")) {
        trimmed = trimmed.substr(2);
    }
    
    try {
        return static_cast<uint32_t>(std::stoul(trimmed, nullptr, 16));
    } catch (const std::exception& e) {
        throw SimulatorException("Failed to parse hex value: " + hexStr);
    }
}

uint64_t parseHex64(const std::string& hexStr) {
    std::string trimmed = trim(hexStr);
    // Remove 0x prefix if present
    if (startsWith(trimmed, "0x") || startsWith(trimmed, "0X")) {
        trimmed = trimmed.substr(2);
    }
    
    try {
        return static_cast<uint64_t>(std::stoull(trimmed, nullptr, 16));
    } catch (const std::exception& e) {
        throw SimulatorException("Failed to parse hex64 value: " + hexStr);
    }
}

uint32_t parseDecimal(const std::string& decStr) {
    std::string trimmed = trim(decStr);
    try {
        return static_cast<uint32_t>(std::stoul(trimmed, nullptr, 10));
    } catch (const std::exception& e) {
        throw SimulatorException("Failed to parse decimal value: " + decStr);
    }
}

bool fileExists(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
}

std::string getFileExtension(const std::string& filename) {
    size_t dotPos = filename.find_last_of('.');
    if (dotPos == std::string::npos) return "";
    return filename.substr(dotPos);
}

std::string getBaseFilename(const std::string& filename) {
    size_t slashPos = filename.find_last_of("/\\");
    std::string baseName = (slashPos == std::string::npos) ? 
                          filename : filename.substr(slashPos + 1);
    
    size_t dotPos = baseName.find_last_of('.');
    if (dotPos == std::string::npos) return baseName;
    
    return baseName.substr(0, dotPos);
}

bool getBit(uint64_t value, uint32_t bit) {
    return (value >> bit) & 1ULL;
}

uint64_t setBit(uint64_t value, uint32_t bit, bool bitValue) {
    if (bitValue) {
        return value | (1ULL << bit);
    } else {
        return value & ~(1ULL << bit);
    }
}

uint64_t extractBits(uint64_t value, uint32_t start, uint32_t width) {
    uint64_t mask = (width == 64) ? ~0ULL : ((1ULL << width) - 1ULL);
    return (value >> start) & mask;
}

uint64_t signExtend(uint64_t value, uint32_t bitWidth) {
    if (bitWidth >= 64) return value;
    
    uint64_t signBit = 1ULL << (bitWidth - 1);
    if (value & signBit) {
        // Negative number, extend the sign bit
        return value | (~0ULL << bitWidth);
    }
    return value;
}

} // namespace HotstateSim
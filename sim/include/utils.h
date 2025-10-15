#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include <string>
#include <vector>

namespace HotstateSim {

// Common utility functions
std::string trim(const std::string& str);
bool startsWith(const std::string& str, const std::string& prefix);
bool endsWith(const std::string& str, const std::string& suffix);
std::vector<std::string> split(const std::string& str, char delimiter);

// Hex string parsing
uint32_t parseHex(const std::string& hexStr);
uint64_t parseHex64(const std::string& hexStr);
uint32_t parseDecimal(const std::string& decStr);

// File utilities
bool fileExists(const std::string& filename);
std::string getFileExtension(const std::string& filename);
std::string getBaseFilename(const std::string& filename);

// Bit manipulation utilities
bool getBit(uint64_t value, uint32_t bit);
uint64_t setBit(uint64_t value, uint32_t bit, bool bitValue);
uint64_t extractBits(uint64_t value, uint32_t start, uint32_t width);
uint64_t signExtend(uint64_t value, uint32_t bitWidth);

// Error handling
class SimulatorException : public std::exception {
private:
    std::string message;
public:
    explicit SimulatorException(const std::string& msg) : message(msg) {}
    const char* what() const noexcept override {
        return message.c_str();
    }
};

} // namespace HotstateSim

#endif // UTILS_H
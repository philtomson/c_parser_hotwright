#include "stimulus_parser.h"
#include "utils.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <stdexcept>

namespace HotstateSim {

bool StimulusParser::loadStimulus(const std::string& filename) {
    if (!fileExists(filename)) {
        throw SimulatorException("Stimulus file not found: " + filename);
    }
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw SimulatorException("Cannot open stimulus file: " + filename);
    }
    
    clear();
    
    std::string line;
    uint32_t lineNumber = 0;
    
    while (std::getline(file, line)) {
        lineNumber++;
        line = trim(line);
        
        // Skip empty lines and comment-only lines
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        try {
            if (!parseLine(line, lineNumber)) {
                std::cerr << "Warning: Failed to parse line " << lineNumber << ": " << line << std::endl;
            }
        } catch (const SimulatorException& e) {
            throw SimulatorException("Error parsing line " + std::to_string(lineNumber) + 
                                   " in " + filename + ": " + e.what());
        }
    }
    
    // Sort entries by cycle
    sortEntries();
    
    // Validate the loaded stimulus
    validate();
    
    loaded = true;
    std::cout << "Loaded " << stimulus.size() << " stimulus entries from " << filename << std::endl;
    
    return true;
}

bool StimulusParser::parseLine(const std::string& line, uint32_t lineNumber) {
    // Extract comment if present
    std::string comment = extractComment(line);
    std::string dataLine = line;
    
    if (!comment.empty()) {
        // Remove comment from the line
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            dataLine = trim(line.substr(0, commentPos));
        }
    }
    
    // Skip empty lines after removing comments
    if (dataLine.empty()) {
        return true;
    }
    
    // Parse the cycle and input values
    std::vector<std::string> parts = split(dataLine, ',');
    
    if (parts.size() < 2) {
        throw SimulatorException("Line must have at least cycle and one input value");
    }
    
    // Parse cycle
    uint32_t cycle = parseCycle(trim(parts[0]));
    
    // Parse input values
    std::vector<std::string> inputParts(parts.begin() + 1, parts.end());
    std::vector<uint8_t> inputs = parseInputValues(join(inputParts, ","));
    
    // Update numInputs if needed
    if (inputs.size() > numInputs) {
        numInputs = inputs.size();
    }
    
    // Create and add the stimulus entry
    stimulus.emplace_back(cycle, inputs, comment);
    
    return true;
}

std::vector<uint8_t> StimulusParser::parseInputValues(const std::string& valuesStr) {
    std::vector<std::string> valueStrings = split(valuesStr, ',');
    std::vector<uint8_t> values;
    
    for (const std::string& valueStr : valueStrings) {
        std::string trimmed = trim(valueStr);
        
        if (trimmed.empty()) {
            continue;
        }
        
        try {
            uint32_t value;
            if (trimmed.find("0x") == 0 || trimmed.find("0X") == 0) {
                value = parseHex(trimmed);
            } else {
                value = parseDecimal(trimmed);
            }
            
            if (value > 255) {
                std::cerr << "Warning: Input value " << value << " exceeds 8-bit range, truncating" << std::endl;
                value = value & 0xFF;
            }
            
            values.push_back(static_cast<uint8_t>(value));
        } catch (const SimulatorException& e) {
            throw SimulatorException("Invalid input value: " + trimmed + " - " + e.what());
        }
    }
    
    return values;
}

uint32_t StimulusParser::parseCycle(const std::string& cycleStr) {
    std::string trimmed = trim(cycleStr);
    
    try {
        if (trimmed.find("0x") == 0 || trimmed.find("0X") == 0) {
            return parseHex(trimmed);
        } else {
            return parseDecimal(trimmed);
        }
    } catch (const SimulatorException& e) {
        throw SimulatorException("Invalid cycle value: " + trimmed + " - " + e.what());
    }
}

std::string StimulusParser::extractComment(const std::string& line) {
    size_t commentPos = line.find('#');
    if (commentPos == std::string::npos) {
        return "";
    }
    
    return trim(line.substr(commentPos + 1));
}

const StimulusEntry* StimulusParser::getEntry(uint32_t cycle) const {
    // Find the entry with the exact cycle
    for (const auto& entry : stimulus) {
        if (entry.cycle == cycle) {
            return &entry;
        }
    }
    return nullptr;
}

std::vector<uint8_t> StimulusParser::getInputs(uint32_t cycle) const {
    const StimulusEntry* entry = getEntry(cycle);
    if (entry) {
        return entry->inputs;
    }
    
    // If no exact match, find the last entry before this cycle
    std::vector<uint8_t> lastInputs(numInputs, 0);
    for (const auto& entry : stimulus) {
        if (entry.cycle <= cycle) {
            lastInputs = entry.inputs;
            // Pad with zeros if needed
            while (lastInputs.size() < numInputs) {
                lastInputs.push_back(0);
            }
        } else {
            break;
        }
    }
    
    return lastInputs;
}

void StimulusParser::printStimulus(size_t maxEntries) const {
    std::cout << "=== Stimulus Entries (" << stimulus.size() << " total) ===" << std::endl;
    std::cout << "Num Inputs: " << numInputs << std::endl;
    
    size_t entriesToShow = std::min(maxEntries, stimulus.size());
    for (size_t i = 0; i < entriesToShow; ++i) {
        printEntry(i);
    }
    
    if (stimulus.size() > maxEntries) {
        std::cout << "... (" << (stimulus.size() - maxEntries) << " more entries)" << std::endl;
    }
    
    std::cout << "========================" << std::endl;
}

void StimulusParser::printEntry(uint32_t index) const {
    if (index >= stimulus.size()) {
        std::cout << "Entry index " << index << " out of range" << std::endl;
        return;
    }
    
    const StimulusEntry& entry = stimulus[index];
    std::cout << "Cycle " << entry.cycle << ": [";
    
    for (size_t i = 0; i < entry.inputs.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << "0x" << std::hex << static_cast<uint32_t>(entry.inputs[i]) << std::dec;
    }
    
    std::cout << "]";
    
    if (!entry.comment.empty()) {
        std::cout << " # " << entry.comment;
    }
    
    std::cout << std::endl;
}

void StimulusParser::validate() const {
    if (stimulus.empty()) {
        throw SimulatorException("No stimulus entries loaded");
    }
    
    // Check for duplicate cycles
    std::vector<uint32_t> cycles;
    for (const auto& entry : stimulus) {
        cycles.push_back(entry.cycle);
    }
    
    std::sort(cycles.begin(), cycles.end());
    for (size_t i = 1; i < cycles.size(); ++i) {
        if (cycles[i] == cycles[i-1]) {
            throw SimulatorException("Duplicate cycle found: " + std::to_string(cycles[i]));
        }
    }
    
    // Check that all entries have the same number of inputs
    if (!stimulus.empty()) {
        size_t expectedInputs = stimulus[0].inputs.size();
        for (size_t i = 1; i < stimulus.size(); ++i) {
            if (stimulus[i].inputs.size() != expectedInputs) {
                std::cerr << "Warning: Entry at cycle " << stimulus[i].cycle 
                         << " has " << stimulus[i].inputs.size() 
                         << " inputs, expected " << expectedInputs << std::endl;
            }
        }
    }
    
    std::cout << "Stimulus validation passed" << std::endl;
}

void StimulusParser::clear() {
    stimulus.clear();
    loaded = false;
    numInputs = 0;
}

void StimulusParser::addEntry(const StimulusEntry& entry) {
    stimulus.push_back(entry);
    if (entry.inputs.size() > numInputs) {
        numInputs = entry.inputs.size();
    }
    loaded = true;
}

void StimulusParser::sortEntries() {
    std::sort(stimulus.begin(), stimulus.end(), 
              [](const StimulusEntry& a, const StimulusEntry& b) {
                  return a.cycle < b.cycle;
              });
}

StimulusParser StimulusParser::createFromVector(const std::vector<std::vector<uint8_t>>& inputSequence) {
    StimulusParser parser;
    
    for (size_t i = 0; i < inputSequence.size(); ++i) {
        StimulusEntry entry(static_cast<uint32_t>(i), inputSequence[i]);
        parser.addEntry(entry);
    }
    
    parser.sortEntries();
    return parser;
}

StimulusParser StimulusParser::createSimple(uint32_t numInputs, uint32_t numCycles) {
    StimulusParser parser;
    parser.setNumInputs(numInputs);
    
    for (uint32_t cycle = 0; cycle < numCycles; ++cycle) {
        std::vector<uint8_t> inputs(numInputs, 0);
        StimulusEntry entry(cycle, inputs);
        parser.addEntry(entry);
    }
    
    parser.sortEntries();
    return parser;
}

std::string join(const std::vector<std::string>& strings, const std::string& delimiter) {
    if (strings.empty()) {
        return "";
    }
    
    std::string result = strings[0];
    for (size_t i = 1; i < strings.size(); ++i) {
        result += delimiter + strings[i];
    }
    
    return result;
}

} // namespace HotstateSim
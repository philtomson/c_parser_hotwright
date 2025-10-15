#ifndef STIMULUS_PARSER_H
#define STIMULUS_PARSER_H

#include <string>
#include <vector>
#include <cstdint>

namespace HotstateSim {

struct StimulusEntry {
    uint32_t cycle;
    std::vector<uint8_t> inputs;
    std::string comment;
    
    StimulusEntry() : cycle(0) {}
    StimulusEntry(uint32_t c, const std::vector<uint8_t>& in, const std::string& comm = "")
        : cycle(c), inputs(in), comment(comm) {}
};

class StimulusParser {
private:
    std::vector<StimulusEntry> stimulus;
    bool loaded = false;
    uint32_t numInputs = 0;
    
    // Helper methods
    bool parseLine(const std::string& line, uint32_t lineNumber);
    std::vector<uint8_t> parseInputValues(const std::string& valuesStr);
    uint32_t parseCycle(const std::string& cycleStr);
    std::string extractComment(const std::string& line);
    
public:
    StimulusParser() = default;
    
    // Load stimulus from file
    bool loadStimulus(const std::string& filename);
    
    // Access methods
    const std::vector<StimulusEntry>& getStimulus() const { return stimulus; }
    size_t size() const { return stimulus.size(); }
    bool isEmpty() const { return stimulus.empty(); }
    
    // Get stimulus for specific cycle
    const StimulusEntry* getEntry(uint32_t cycle) const;
    std::vector<uint8_t> getInputs(uint32_t cycle) const;
    
    // Configuration
    void setNumInputs(uint32_t num) { numInputs = num; }
    uint32_t getNumInputs() const { return numInputs; }
    
    // Status
    bool isLoaded() const { return loaded; }
    
    // Debug
    void printStimulus(size_t maxEntries = 16) const;
    void printEntry(uint32_t index) const;
    void validate() const;
    
    // Utility methods
    void clear();
    void addEntry(const StimulusEntry& entry);
    void sortEntries();
    
    // Static utility methods
    static StimulusParser createFromVector(const std::vector<std::vector<uint8_t>>& inputSequence);
    static StimulusParser createSimple(uint32_t numInputs, uint32_t numCycles);
};

// Utility function declarations
std::string join(const std::vector<std::string>& strings, const std::string& delimiter);

} // namespace HotstateSim

#endif // STIMULUS_PARSER_H
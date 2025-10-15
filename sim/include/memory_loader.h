#ifndef MEMORY_LOADER_H
#define MEMORY_LOADER_H

#include <string>
#include <vector>
#include <cstdint>

namespace HotstateSim {

// Structure to hold parameters from .vh files
struct Parameters {
    uint32_t STATE_WIDTH = 0;
    uint32_t MASK_WIDTH = 0;
    uint32_t JADR_WIDTH = 0;
    uint32_t VARSEL_WIDTH = 0;
    uint32_t TIMERSEL_WIDTH = 0;
    uint32_t TIMERLD_WIDTH = 0;
    uint32_t SWITCH_SEL_WIDTH = 0;
    uint32_t SWITCH_ADR_WIDTH = 0;
    uint32_t STATE_CAPTURE_WIDTH = 0;
    uint32_t VAR_OR_TIMER_WIDTH = 0;
    uint32_t BRANCH_WIDTH = 0;
    uint32_t FORCED_JMP_WIDTH = 0;
    uint32_t SUB_WIDTH = 0;
    uint32_t RTN_WIDTH = 0;
    uint32_t INSTR_WIDTH = 0;
    
    // Additional parameters from hotstate.sv
    uint32_t NUM_STATES = 0;
    uint32_t NUM_VARSEL = 0;
    uint32_t NUM_VARSEL_BITS = 0;
    uint32_t NUM_VARS = 0;
    uint32_t NUM_TIMERS = 0;
    uint32_t NUM_SWITCHES = 0;
    uint32_t SWITCH_OFFSET_BITS = 0;
    uint32_t SWITCH_MEM_WORDS = 0;
    uint32_t NUM_SWITCH_BITS = 0;
    uint32_t NUM_ADR_BITS = 0;
    uint32_t NUM_WORDS = 0;
    uint32_t TIM_WIDTH = 0;
    uint32_t TIM_MEM_WORDS = 0;
    uint32_t NUM_CTL_BITS = 0;
    uint32_t SMDATA_WIDTH = 0;
    uint32_t STACK_DEPTH = 0;
    
    bool isValid() const;
    void print() const;
};

class MemoryLoader {
private:
    std::vector<uint32_t> vardata;
    std::vector<uint32_t> switchdata;
    std::vector<uint64_t> smdata;
    Parameters params;
    bool loaded = false;
    
    // Helper methods
    bool loadMemoryFile(const std::string& filename, std::vector<uint32_t>& data);
    bool loadSmdataFile(const std::string& filename, std::vector<uint64_t>& data);
    bool parseParameterFile(const std::string& filename);
    std::string extractParameterValue(const std::string& line);
    
public:
    MemoryLoader() = default;
    
    // Load memory files from a base path
    bool loadFromBasePath(const std::string& basePath);
    
    // Individual file loading methods
    bool loadVardata(const std::string& filename);
    bool loadSwitchdata(const std::string& filename);
    bool loadSmdata(const std::string& filename);
    bool loadParams(const std::string& filename);
    
    // Access methods
    const std::vector<uint32_t>& getVardata() const { return vardata; }
    const std::vector<uint32_t>& getSwitchdata() const { return switchdata; }
    const std::vector<uint64_t>& getSmdata() const { return smdata; }
    const Parameters& getParams() const { return params; }
    
    // Status
    bool isLoaded() const { return loaded; }
    size_t getVardataSize() const { return vardata.size(); }
    size_t getSwitchdataSize() const { return switchdata.size(); }
    size_t getSmdataSize() const { return smdata.size(); }
    
    // Debug
    void printMemoryInfo() const;
    void printVardata(size_t maxEntries = 16) const;
    void printSwitchdata(size_t maxEntries = 16) const;
    void printSmdata(size_t maxEntries = 16) const;
    void printParams() const { params.print(); }
};

} // namespace HotstateSim

#endif // MEMORY_LOADER_H
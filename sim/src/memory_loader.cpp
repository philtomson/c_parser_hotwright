#include "memory_loader.h"
#include "utils.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace HotstateSim {

bool Parameters::isValid() const {
    return STATE_WIDTH > 0 && 
           MASK_WIDTH > 0 && 
           JADR_WIDTH > 0 && 
           NUM_STATES > 0 && 
           NUM_VARS > 0 && 
           NUM_ADR_BITS > 0 && 
           NUM_WORDS > 0;
}

void Parameters::print() const {
    std::cout << "=== Parameters ===" << std::endl;
    std::cout << "STATE_WIDTH: " << STATE_WIDTH << std::endl;
    std::cout << "MASK_WIDTH: " << MASK_WIDTH << std::endl;
    std::cout << "JADR_WIDTH: " << JADR_WIDTH << std::endl;
    std::cout << "VARSEL_WIDTH: " << VARSEL_WIDTH << std::endl;
    std::cout << "NUM_STATES: " << NUM_STATES << std::endl;
    std::cout << "NUM_VARS: " << NUM_VARS << std::endl;
    std::cout << "NUM_ADR_BITS: " << NUM_ADR_BITS << std::endl;
    std::cout << "NUM_WORDS: " << NUM_WORDS << std::endl;
    std::cout << "NUM_SWITCHES: " << NUM_SWITCHES << std::endl;
    std::cout << "NUM_TIMERS: " << NUM_TIMERS << std::endl;
    std::cout << "INSTR_WIDTH: " << INSTR_WIDTH << std::endl;
    std::cout << "SMDATA_WIDTH: " << SMDATA_WIDTH << std::endl;
    std::cout << "=================" << std::endl;
}

bool MemoryLoader::loadFromBasePath(const std::string& basePath) {
    std::string baseFilename = getBaseFilename(basePath);
    
    bool success = true;
    success &= loadVardata(baseFilename + "_vardata.mem");
    success &= loadSwitchdata(baseFilename + "_switchdata.mem");
    success &= loadSmdata(baseFilename + "_smdata.mem");
    success &= loadParams(baseFilename + "_params.vh");

    // Try to load symbol table (TOML format preferred, with fallback to text format)
    if (!loadSymbolTable(baseFilename + "_symbols.toml")) {
        // Fallback to old text format for backward compatibility
        loadSymbolTable(baseFilename + "_symbols.txt");
    }
    
    if (success) {
        loaded = true;
        std::cout << "Successfully loaded memory files from base: " << baseFilename << std::endl;
    } else {
        std::cerr << "Failed to load some memory files from base: " << baseFilename << std::endl;
    }
    
    return success;
}

bool MemoryLoader::loadVardata(const std::string& filename) {
    try {
        return loadMemoryFile(filename, vardata);
    } catch (const SimulatorException& e) {
        std::cerr << "Error loading vardata from " << filename << ": " << e.what() << std::endl;
        return false;
    }
}

bool MemoryLoader::loadSwitchdata(const std::string& filename) {
    try {
        return loadMemoryFile(filename, switchdata);
    } catch (const SimulatorException& e) {
        std::cerr << "Error loading switchdata from " << filename << ": " << e.what() << std::endl;
        return false;
    }
}

bool MemoryLoader::loadSmdata(const std::string& filename) {
    try {
        return loadSmdataFile(filename, smdata);
    } catch (const SimulatorException& e) {
        std::cerr << "Error loading smdata from " << filename << ": " << e.what() << std::endl;
        return false;
    }
}

bool MemoryLoader::loadParams(const std::string& filename) {
    try {
        return parseParameterFile(filename);
    } catch (const SimulatorException& e) {
        std::cerr << "Error loading params from " << filename << ": " << e.what() << std::endl;
        return false;
    }
}

bool MemoryLoader::loadMemoryFile(const std::string& filename, std::vector<uint32_t>& data) {
    if (!fileExists(filename)) {
        throw SimulatorException("File not found: " + filename);
    }
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw SimulatorException("Cannot open file: " + filename);
    }
    
    data.clear();
    std::string line;
    uint32_t lineNumber = 0;
    
    while (std::getline(file, line)) {
        lineNumber++;
        line = trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == '/') {
            continue;
        }
        
        try {
            // Try to parse as hex first, then decimal
            uint32_t value;
            if (line.find("0x") == 0 || line.find("0X") == 0) {
                value = parseHex(line);
            } else if (line.find_first_not_of("0123456789") == std::string::npos) {
                // Pure decimal
                value = parseDecimal(line);
            } else {
                // Try hex parsing
                value = parseHex(line);
            }
            data.push_back(value);
        } catch (const SimulatorException& e) {
            throw SimulatorException("Error parsing line " + std::to_string(lineNumber) + 
                                   " in " + filename + ": " + e.what());
        }
    }
    
    std::cout << "Loaded " << data.size() << " values from " << filename << std::endl;
    return true;
}

bool MemoryLoader::loadSmdataFile(const std::string& filename, std::vector<uint64_t>& data) {
    if (!fileExists(filename)) {
        throw SimulatorException("File not found: " + filename);
    }
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw SimulatorException("Cannot open file: " + filename);
    }
    
    data.clear();
    std::string line;
    uint32_t lineNumber = 0;
    
    while (std::getline(file, line)) {
        lineNumber++;
        line = trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == '/') {
            continue;
        }
        
        try {
            // Parse as 64-bit hex
            uint64_t value = parseHex64(line);
            data.push_back(value);
        } catch (const SimulatorException& e) {
            throw SimulatorException("Error parsing line " + std::to_string(lineNumber) + 
                                   " in " + filename + ": " + e.what());
        }
    }
    
    std::cout << "Loaded " << data.size() << " microcode instructions from " << filename << std::endl;
    return true;
}

bool MemoryLoader::parseParameterFile(const std::string& filename) {
    if (!fileExists(filename)) {
        throw SimulatorException("File not found: " + filename);
    }
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw SimulatorException("Cannot open file: " + filename);
    }
    
    std::string line;
    uint32_t lineNumber = 0;
    
    while (std::getline(file, line)) {
        lineNumber++;
        line = trim(line);
        
        // Skip empty lines, comments, and preprocessor directives
        if (line.empty() || line[0] == '#' || line[0] == '/' || startsWith(line, "`")) {
            continue;
        }
        
        // Look for parameter definitions
        if (line.find("localparam") != std::string::npos) {
            // Extract parameter name and value
            size_t paramStart = line.find("localparam") + 10;
            std::string paramPart = trim(line.substr(paramStart));

            size_t equalsPos = paramPart.find('=');
            if (equalsPos == std::string::npos) continue;

            std::string paramName = trim(paramPart.substr(0, equalsPos));
            std::string paramValue = extractParameterValue(paramPart.substr(equalsPos + 1));

            // Skip if this is a preprocessor directive or comment
            if (paramName.empty() || paramName[0] == '`' || paramName[0] == '/') {
                continue;
            }

            // Debug output
            std::cout << "DEBUG: Parsing parameter: '" << paramName << "' = '" << paramValue << "'" << std::endl;

            // Parse the parameter value (skip if it's an expression)
            uint32_t value;
            if (paramValue.find_first_not_of("0123456789") != std::string::npos) {
                // Contains non-numeric characters, likely an expression
                std::cout << "DEBUG: Skipping expression parameter: " << paramName << " = " << paramValue << std::endl;
                continue;
            }
            value = parseDecimal(paramValue);

            // Store the parameter
            if (paramName == "STATE_WIDTH") params.STATE_WIDTH = value;
            else if (paramName == "MASK_WIDTH") params.MASK_WIDTH = value;
            else if (paramName == "JADR_WIDTH") params.JADR_WIDTH = value;
            else if (paramName == "VARSEL_WIDTH") params.VARSEL_WIDTH = value;
            else if (paramName == "TIMERSEL_WIDTH") params.TIMERSEL_WIDTH = value;
            else if (paramName == "TIMERLD_WIDTH") params.TIMERLD_WIDTH = value;
            else if (paramName == "SWITCH_SEL_WIDTH") params.SWITCH_SEL_WIDTH = value;
            else if (paramName == "SWITCH_ADR_WIDTH") params.SWITCH_ADR_WIDTH = value;
            else if (paramName == "STATE_CAPTURE_WIDTH") params.STATE_CAPTURE_WIDTH = value;
            else if (paramName == "VAR_OR_TIMER_WIDTH") params.VAR_OR_TIMER_WIDTH = value;
            else if (paramName == "BRANCH_WIDTH") params.BRANCH_WIDTH = value;
            else if (paramName == "FORCED_JMP_WIDTH") params.FORCED_JMP_WIDTH = value;
            else if (paramName == "SUB_WIDTH") params.SUB_WIDTH = value;
            else if (paramName == "RTN_WIDTH") params.RTN_WIDTH = value;
            else if (paramName == "INSTR_WIDTH") params.INSTR_WIDTH = value;
            // Parameters are now stored in the unified section below
            else {
                std::cout << "DEBUG: Unknown parameter: " << paramName << " = " << value << std::endl;
            }
            
            // Store the parameter
            if (paramName == "STATE_WIDTH") params.STATE_WIDTH = value;
            else if (paramName == "MASK_WIDTH") params.MASK_WIDTH = value;
            else if (paramName == "JADR_WIDTH") params.JADR_WIDTH = value;
            else if (paramName == "VARSEL_WIDTH") params.VARSEL_WIDTH = value;
            else if (paramName == "TIMERSEL_WIDTH") params.TIMERSEL_WIDTH = value;
            else if (paramName == "TIMERLD_WIDTH") params.TIMERLD_WIDTH = value;
            else if (paramName == "SWITCH_SEL_WIDTH") params.SWITCH_SEL_WIDTH = value;
            else if (paramName == "SWITCH_ADR_WIDTH") params.SWITCH_ADR_WIDTH = value;
            else if (paramName == "STATE_CAPTURE_WIDTH") params.STATE_CAPTURE_WIDTH = value;
            else if (paramName == "VAR_OR_TIMER_WIDTH") params.VAR_OR_TIMER_WIDTH = value;
            else if (paramName == "BRANCH_WIDTH") params.BRANCH_WIDTH = value;
            else if (paramName == "FORCED_JMP_WIDTH") params.FORCED_JMP_WIDTH = value;
            else if (paramName == "SUB_WIDTH") params.SUB_WIDTH = value;
            else if (paramName == "RTN_WIDTH") params.RTN_WIDTH = value;
            else if (paramName == "INSTR_WIDTH") params.INSTR_WIDTH = value;
            else if (paramName == "NUM_STATES") params.NUM_STATES = value;
            else if (paramName == "NUM_VARSEL") params.NUM_VARSEL = value;
            else if (paramName == "NUM_VARSEL_BITS") params.NUM_VARSEL_BITS = value;
            else if (paramName == "NUM_VARS") params.NUM_VARS = value;
            else if (paramName == "NUM_TIMERS") params.NUM_TIMERS = value;
            else if (paramName == "NUM_SWITCHES") params.NUM_SWITCHES = value;
            else if (paramName == "SWITCH_OFFSET_BITS") params.SWITCH_OFFSET_BITS = value;
            else if (paramName == "SWITCH_MEM_WORDS") params.SWITCH_MEM_WORDS = value;
            else if (paramName == "NUM_SWITCH_BITS") params.NUM_SWITCH_BITS = value;
            else if (paramName == "NUM_ADR_BITS") params.NUM_ADR_BITS = value;
            else if (paramName == "NUM_WORDS") params.NUM_WORDS = value;
            else if (paramName == "TIM_WIDTH") params.TIM_WIDTH = value;
            else if (paramName == "TIM_MEM_WORDS") params.TIM_MEM_WORDS = value;
            else if (paramName == "NUM_CTL_BITS") params.NUM_CTL_BITS = value;
            else if (paramName == "SMDATA_WIDTH") params.SMDATA_WIDTH = value;
            else if (paramName == "STACK_DEPTH") params.STACK_DEPTH = value;
        }
    }
    
    // Calculate derived parameters if not explicitly set
    if (params.INSTR_WIDTH == 0) {
        params.INSTR_WIDTH = params.STATE_WIDTH + params.MASK_WIDTH + params.JADR_WIDTH + 
                           params.VARSEL_WIDTH + params.TIMERSEL_WIDTH + params.TIMERLD_WIDTH + 
                           params.SWITCH_SEL_WIDTH + params.SWITCH_ADR_WIDTH + 
                           params.STATE_CAPTURE_WIDTH + params.VAR_OR_TIMER_WIDTH + 
                           params.BRANCH_WIDTH + params.FORCED_JMP_WIDTH + params.SUB_WIDTH + 
                           params.RTN_WIDTH;
    }
    
    if (params.NUM_CTL_BITS == 0) {
        params.NUM_CTL_BITS = params.NUM_ADR_BITS + params.NUM_VARSEL_BITS + 
                            (2 * params.NUM_TIMERS) + params.NUM_SWITCHES + 7;
    }
    
    if (params.SMDATA_WIDTH == 0) {
        params.SMDATA_WIDTH = (2 * params.NUM_STATES) + params.NUM_CTL_BITS;
    }

    // Calculate derived parameters if not explicitly set
    if (params.NUM_STATES == 0) {
        // Calculate from STATE_WIDTH: NUM_STATES = 2^STATE_WIDTH
        params.NUM_STATES = 1 << params.STATE_WIDTH;
        std::cout << "DEBUG: Calculated NUM_STATES = " << params.NUM_STATES << " from STATE_WIDTH" << std::endl;
    }

    if (params.NUM_VARS == 0) {
        // Estimate from vardata size or use a reasonable default
        if (!vardata.empty()) {
            params.NUM_VARS = std::min(static_cast<uint32_t>(vardata.size()), static_cast<uint32_t>(8));
        } else {
            params.NUM_VARS = 3; // Default based on the switches.c example
        }
        std::cout << "DEBUG: Estimated NUM_VARS = " << params.NUM_VARS << " from vardata size" << std::endl;
    }

    if (params.NUM_ADR_BITS == 0) {
        // Calculate from NUM_WORDS: NUM_ADR_BITS = log2(NUM_WORDS)
        if (params.NUM_WORDS > 0) {
            params.NUM_ADR_BITS = 0;
            uint32_t temp = params.NUM_WORDS - 1;
            while (temp > 0) {
                temp >>= 1;
                params.NUM_ADR_BITS++;
            }
        } else {
            params.NUM_ADR_BITS = 6; // Default
        }
        std::cout << "DEBUG: Calculated NUM_ADR_BITS = " << params.NUM_ADR_BITS << std::endl;
    }

    if (params.NUM_WORDS == 0) {
        // Estimate from smdata size
        if (!smdata.empty()) {
            params.NUM_WORDS = std::min(static_cast<uint32_t>(smdata.size()), static_cast<uint32_t>(64));
        } else {
            params.NUM_WORDS = 32; // Default
        }
        std::cout << "DEBUG: Estimated NUM_WORDS = " << params.NUM_WORDS << std::endl;
    }

    std::cout << "Loaded parameters from " << filename << std::endl;
    return true;
}

std::string MemoryLoader::extractParameterValue(const std::string& valueStr) {
    std::string trimmed = trim(valueStr);
    
    // Remove trailing semicolon if present
    if (!trimmed.empty() && trimmed.back() == ';') {
        trimmed = trimmed.substr(0, trimmed.length() - 1);
    }
    
    // Handle multi-line expressions (like INSTR_WIDTH = STATE_WIDTH + ...)
    if (trimmed.find('\n') != std::string::npos) {
        // For multi-line expressions, we need to extract just the first line
        size_t newlinePos = trimmed.find('\n');
        trimmed = trim(trimmed.substr(0, newlinePos));
    }
    
    // Handle expressions like PARAM1 + PARAM2
    // For now, just return the first part before any operator
    size_t opPos = trimmed.find_first_of("+-*/");
    if (opPos != std::string::npos) {
        trimmed = trim(trimmed.substr(0, opPos));
    }
    
    return trimmed;
}

void MemoryLoader::printMemoryInfo() const {
    std::cout << "=== Memory Information ===" << std::endl;
    std::cout << "Vardata size: " << vardata.size() << " entries" << std::endl;
    std::cout << "Switchdata size: " << switchdata.size() << " entries" << std::endl;
    std::cout << "Smdata size: " << smdata.size() << " entries" << std::endl;
    std::cout << "Loaded: " << (loaded ? "Yes" : "No") << std::endl;
    std::cout << "========================" << std::endl;
}

void MemoryLoader::printVardata(size_t maxEntries) const {
    std::cout << "=== Vardata (" << vardata.size() << " entries) ===" << std::endl;
    size_t entriesToShow = std::min(maxEntries, vardata.size());
    for (size_t i = 0; i < entriesToShow; ++i) {
        std::cout << "[" << i << "] = 0x" << std::hex << vardata[i] << std::dec << std::endl;
    }
    if (vardata.size() > maxEntries) {
        std::cout << "... (" << (vardata.size() - maxEntries) << " more entries)" << std::endl;
    }
    std::cout << "========================" << std::endl;
}

void MemoryLoader::printSwitchdata(size_t maxEntries) const {
    std::cout << "=== Switchdata (" << switchdata.size() << " entries) ===" << std::endl;
    size_t entriesToShow = std::min(maxEntries, switchdata.size());
    for (size_t i = 0; i < entriesToShow; ++i) {
        std::cout << "[" << i << "] = " << switchdata[i] << std::endl;
    }
    if (switchdata.size() > maxEntries) {
        std::cout << "... (" << (switchdata.size() - maxEntries) << " more entries)" << std::endl;
    }
    std::cout << "===========================" << std::endl;
}

void MemoryLoader::printSmdata(size_t maxEntries) const {
    std::cout << "=== Smdata (" << smdata.size() << " entries) ===" << std::endl;
    size_t entriesToShow = std::min(maxEntries, smdata.size());
    for (size_t i = 0; i < entriesToShow; ++i) {
        std::cout << "[" << i << "] = 0x" << std::hex << smdata[i] << std::dec << std::endl;
    }
    if (smdata.size() > maxEntries) {
        std::cout << "... (" << (smdata.size() - maxEntries) << " more entries)" << std::endl;
    }
    std::cout << "========================" << std::endl;
}

// Load symbol table file (supports both TOML and text formats for backward compatibility)
bool MemoryLoader::loadSymbolTable(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        // Symbol table is optional, so don't treat this as an error
        std::cout << "DEBUG: Symbol table file not found: " << filename << " (optional)" << std::endl;
        return false;
    }

    // Check if this is a TOML file by looking for TOML section headers
    std::string first_line, second_line;
    std::getline(file, first_line);
    std::getline(file, second_line);

    file.close();

    // Check for TOML format indicators
    bool is_toml = (first_line.find("[metadata]") != std::string::npos ||
                   first_line.find("[state_variables]") != std::string::npos ||
                   first_line.find("[input_variables]") != std::string::npos ||
                   second_line.find("[metadata]") != std::string::npos ||
                   second_line.find("[state_variables]") != std::string::npos ||
                   second_line.find("[input_variables]") != std::string::npos);

    if (is_toml) {
        return loadSymbolTableTOML(filename);
    } else {
        return loadSymbolTableText(filename);
    }
}

// TOML format parser
bool MemoryLoader::loadSymbolTableTOML(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    std::string current_section;
    bool in_state_vars = false;
    bool in_input_vars = false;

    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Check for section headers
        if (line.find("[state_variables]") == 0) {
            current_section = "state_variables";
            in_state_vars = true;
            in_input_vars = false;
            continue;
        } else if (line.find("[input_variables]") == 0) {
            current_section = "input_variables";
            in_state_vars = false;
            in_input_vars = true;
            continue;
        }

        // Parse variable entries (format: "index" = { name = "var_name", type = "input/output" })
        if ((in_state_vars || in_input_vars) && line.find('=') != std::string::npos) {
            // Extract index (quoted number before =)
            size_t quote_start = line.find('"');
            if (quote_start == std::string::npos) continue;
            size_t quote_end = line.find('"', quote_start + 1);
            if (quote_end == std::string::npos) continue;

            std::string index_str = line.substr(quote_start + 1, quote_end - quote_start - 1);
            uint32_t index = std::stoul(index_str);

            // Extract name (from name = "value")
            size_t name_start = line.find("name = \"");
            if (name_start == std::string::npos) continue;
            name_start += 8; // Skip "name = \""
            size_t name_end = line.find('"', name_start);
            if (name_end == std::string::npos) continue;

            std::string name = line.substr(name_start, name_end - name_start);

            // Add to appropriate map
            if (in_state_vars) {
                stateNameToIndex[name] = index;
                stateIndexToName[index] = name;
            } else if (in_input_vars) {
                inputNameToIndex[name] = index;
                inputIndexToName[index] = name;
            }
        }
    }

    file.close();

    if (!inputNameToIndex.empty() || !stateNameToIndex.empty()) {
        std::cout << "Loaded TOML symbol table from " << filename << std::endl;
        std::cout << "  Input variables: " << inputNameToIndex.size() << std::endl;
        std::cout << "  State variables: " << stateNameToIndex.size() << std::endl;
    }

    return true;
}

// Text format parser (original implementation for backward compatibility)
bool MemoryLoader::loadSymbolTableText(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    int lineNumber = 0;

    while (std::getline(file, line)) {
        lineNumber++;
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream iss(line);
        std::string type, name;
        uint32_t index;

        if (!(iss >> type >> index >> name)) {
            std::cerr << "Warning: Invalid symbol table format at line " << lineNumber << ": " << line << std::endl;
            continue;
        }

        if (type == "INPUT") {
            inputNameToIndex[name] = index;
            inputIndexToName[index] = name;
        } else if (type == "STATE") {
            stateNameToIndex[name] = index;
            stateIndexToName[index] = name;
        } else {
            std::cerr << "Warning: Unknown symbol type at line " << lineNumber << ": " << type << std::endl;
        }
    }

    file.close();

    if (!inputNameToIndex.empty() || !stateNameToIndex.empty()) {
        std::cout << "Loaded text symbol table from " << filename << std::endl;
        std::cout << "  Input variables: " << inputNameToIndex.size() << std::endl;
        std::cout << "  State variables: " << stateNameToIndex.size() << std::endl;
    }

    return true;
}

// Symbol table accessor methods
uint32_t MemoryLoader::getInputIndexByName(const std::string& name) const {
    auto it = inputNameToIndex.find(name);
    return (it != inputNameToIndex.end()) ? it->second : UINT32_MAX;
}

uint32_t MemoryLoader::getStateIndexByName(const std::string& name) const {
    auto it = stateNameToIndex.find(name);
    return (it != stateNameToIndex.end()) ? it->second : UINT32_MAX;
}

const std::string& MemoryLoader::getInputNameByIndex(uint32_t index) const {
    auto it = inputIndexToName.find(index);
    static std::string empty = "";
    return (it != inputIndexToName.end()) ? it->second : empty;
}

const std::string& MemoryLoader::getStateNameByIndex(uint32_t index) const {
    auto it = stateIndexToName.find(index);
    static std::string empty = "";
    return (it != stateIndexToName.end()) ? it->second : empty;
}

} // namespace HotstateSim
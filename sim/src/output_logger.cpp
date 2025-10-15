#include "output_logger.h"
#include "utils.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <bitset>

namespace HotstateSim {

OutputLogger::OutputLogger()
    : format(OutputFormat::CONSOLE)
    , filename("")
    , fileOpen(false)
    , realTime(true)
    , maxLogEntries(10000)
    , vcdHeaderWritten(false)
{
}

OutputLogger::~OutputLogger() {
    closeFile();
}

void OutputLogger::logCycle(uint32_t cycle, const HotstateModel& model, const std::vector<uint8_t>& inputs) {
    LogEntry entry;
    entry.cycle = cycle;
    entry.address = model.getCurrentAddress();
    entry.states = model.getStates();
    entry.outputs = model.getOutputs();
    entry.inputs = inputs;
    entry.ready = model.isReady();
    entry.lhs = model.getLhs();
    entry.fired = model.getFired();
    entry.jmpadr = model.getJmpadr();
    entry.switchActive = model.getSwitchActive();
    
    logEntry(entry, model);
}

void OutputLogger::logEntry(const LogEntry& entry, const HotstateModel& model) {
    // Add to log entries
    logEntries.push_back(entry);
    
    // Limit log size if needed
    if (maxLogEntries > 0 && logEntries.size() > maxLogEntries) {
        logEntries.erase(logEntries.begin());
    }
    
    // Write output based on format
    switch (format) {
        case OutputFormat::CONSOLE:
            if (realTime) {
                writeConsoleEntry(entry, model);
            }
            break;
        case OutputFormat::VCD:
            if (fileOpen) {
                writeVCDEntry(entry, model);
            }
            break;
        case OutputFormat::CSV:
            if (fileOpen) {
                writeCSVEntry(entry, model);
            }
            break;
        case OutputFormat::JSON:
            if (fileOpen) {
                writeJSONEntry(entry, model);
            }
            break;
    }
}

void OutputLogger::logEntry(const LogEntry& entry) {
    logEntries.push_back(entry);
    
    // Limit log size if needed
    if (maxLogEntries > 0 && logEntries.size() > maxLogEntries) {
        logEntries.erase(logEntries.begin());
    }
}

void OutputLogger::writeConsoleEntry(const LogEntry& entry, const HotstateModel& model) {
    std::cout << "Cycle: " << std::setw(6) << entry.cycle 
              << ", Addr: 0x" << std::hex << std::setw(4) << entry.address << std::dec
              << ", Ready: " << (entry.ready ? "1" : "0")
              << ", LHS: " << (entry.lhs ? "1" : "0")
              << ", Fired: " << (entry.fired ? "1" : "0")
              << ", States: ";
    
    for (size_t i = 0; i < entry.states.size(); ++i) {
        std::cout << entry.states[i];
    }
    
    std::cout << ", Outputs: [";
    for (size_t i = 0; i < entry.outputs.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << "0x" << std::hex << static_cast<uint32_t>(entry.outputs[i]) << std::dec;
    }
    
    if (!entry.inputs.empty()) {
        std::cout << "], Inputs: [";
        for (size_t i = 0; i < entry.inputs.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << "0x" << std::hex << static_cast<uint32_t>(entry.inputs[i]) << std::dec;
        }
    }
    
    std::cout << "]" << std::endl;
}

void OutputLogger::writeVCDEntry(const LogEntry& entry, const HotstateModel& model) {
    if (!vcdHeaderWritten) {
        writeVCDHeader(model);
        vcdHeaderWritten = true;
    }
    
    // Write time
    writeVCDTime(entry.cycle);
    
    // Write signal changes
    for (size_t i = 0; i < entry.states.size(); ++i) {
        writeVCDValueChange(vcdSignalCodes[i], boolToVCDString(entry.states[i]));
    }
    
    // Write address
    writeVCDValueChange(vcdSignalCodes[entry.states.size()], uint8ToVCDString(entry.address & 0xFF));
    
    // Write control signals
    size_t controlOffset = entry.states.size() + 1;
    writeVCDValueChange(vcdSignalCodes[controlOffset], boolToVCDString(entry.ready));
    writeVCDValueChange(vcdSignalCodes[controlOffset + 1], boolToVCDString(entry.lhs));
    writeVCDValueChange(vcdSignalCodes[controlOffset + 2], boolToVCDString(entry.fired));
}

void OutputLogger::writeCSVEntry(const LogEntry& entry, const HotstateModel& model) {
    file << entry.cycle << "," << entry.address << "," << (entry.ready ? "1" : "0") 
         << "," << (entry.lhs ? "1" : "0") << "," << (entry.fired ? "1" : "0");
    
    // Write states
    for (bool state : entry.states) {
        file << "," << (state ? "1" : "0");
    }
    
    // Write outputs
    for (uint8_t output : entry.outputs) {
        file << ",0x" << std::hex << static_cast<uint32_t>(output) << std::dec;
    }
    
    // Write inputs
    for (uint8_t input : entry.inputs) {
        file << ",0x" << std::hex << static_cast<uint32_t>(input) << std::dec;
    }
    
    file << std::endl;
}

void OutputLogger::writeJSONEntry(const LogEntry& entry, const HotstateModel& model) {
    file << "{" << std::endl;
    file << "  \"cycle\": " << entry.cycle << "," << std::endl;
    file << "  \"address\": " << entry.address << "," << std::endl;
    file << "  \"ready\": " << (entry.ready ? "true" : "false") << "," << std::endl;
    file << "  \"lhs\": " << (entry.lhs ? "true" : "false") << "," << std::endl;
    file << "  \"fired\": " << (entry.fired ? "true" : "false") << "," << std::endl;
    
    // Write states
    file << "  \"states\": [";
    for (size_t i = 0; i < entry.states.size(); ++i) {
        if (i > 0) file << ", ";
        file << (entry.states[i] ? "true" : "false");
    }
    file << "]," << std::endl;
    
    // Write outputs
    file << "  \"outputs\": [";
    for (size_t i = 0; i < entry.outputs.size(); ++i) {
        if (i > 0) file << ", ";
        file << static_cast<uint32_t>(entry.outputs[i]);
    }
    file << "]," << std::endl;
    
    // Write inputs
    file << "  \"inputs\": [";
    for (size_t i = 0; i < entry.inputs.size(); ++i) {
        if (i > 0) file << ", ";
        file << static_cast<uint32_t>(entry.inputs[i]);
    }
    file << "]" << std::endl;
    
    file << "}" << std::endl;
}

void OutputLogger::writeVCDHeader(const HotstateModel& model) {
    file << "$timescale 1ns $end" << std::endl;
    file << "$scope module hotstate $end" << std::endl;
    
    // Generate signal names and codes
    vcdSignalNames.clear();
    vcdSignalCodes.clear();
    
    // Add state signals
    for (size_t i = 0; i < model.getStates().size(); ++i) {
        std::string name = "state[" + std::to_string(i) + "]";
        std::string code = generateVCDCode(i);
        vcdSignalNames.push_back(name);
        vcdSignalCodes.push_back(code);
        file << "$var wire 1 " << code << " " << name << " $end" << std::endl;
    }
    
    // Add address signal
    std::string addrCode = generateVCDCode(vcdSignalNames.size());
    vcdSignalNames.push_back("address");
    vcdSignalCodes.push_back(addrCode);
    file << "$var wire 8 " << addrCode << " address $end" << std::endl;
    
    // Add control signals
    std::vector<std::string> controlNames = {"ready", "lhs", "fired", "jmpadr", "switch_active"};
    for (const std::string& name : controlNames) {
        std::string code = generateVCDCode(vcdSignalNames.size());
        vcdSignalNames.push_back(name);
        vcdSignalCodes.push_back(code);
        file << "$var wire 1 " << code << " " << name << " $end" << std::endl;
    }
    
    file << "$upscope $end" << std::endl;
    file << "$enddefinitions $end" << std::endl;
    file << "$dumpvars" << std::endl;
    
    // Write initial values
    for (size_t i = 0; i < vcdSignalCodes.size(); ++i) {
        writeVCDValueChange(vcdSignalCodes[i], "0");
    }
    
    file << "$end" << std::endl;
}

void OutputLogger::writeVCDValueChange(const std::string& code, const std::string& value) {
    file << value << code << std::endl;
}

void OutputLogger::writeVCDTime(uint64_t time) {
    file << "#" << time << std::endl;
}

std::string OutputLogger::generateVCDCode(uint32_t index) {
    // Generate VCD variable codes (ASCII printable characters)
    std::string code;
    uint32_t value = index + 33; // Start from '!' (ASCII 33)
    
    while (value > 0) {
        char c = static_cast<char>((value % 94) + 33); // Printable ASCII range
        code = c + code;
        value = value / 94;
    }
    
    return code;
}

std::string OutputLogger::boolToVCDString(bool value) {
    return value ? "1" : "0";
}

std::string OutputLogger::uint8ToVCDString(uint8_t value) {
    std::stringstream ss;
    ss << "b" << std::bitset<8>(value) << " ";
    return ss.str();
}

bool OutputLogger::openFile() {
    if (filename.empty()) {
        return false;
    }
    
    file.open(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open output file: " << filename << std::endl;
        return false;
    }
    
    fileOpen = true;
    vcdHeaderWritten = false;
    
    // Write header for CSV and JSON formats
    if (format == OutputFormat::CSV) {
        file << "Cycle,Address,Ready,LHS,Fired";
        // Add state columns
        for (size_t i = 0; i < 8; ++i) { // Assume max 8 states for header
            file << ",State" << i;
        }
        // Add output columns
        for (size_t i = 0; i < 8; ++i) { // Assume max 8 outputs for header
            file << ",Output" << i;
        }
        // Add input columns
        for (size_t i = 0; i < 8; ++i) { // Assume max 8 inputs for header
            file << ",Input" << i;
        }
        file << std::endl;
    } else if (format == OutputFormat::JSON) {
        file << "[" << std::endl;
    }
    
    return true;
}

void OutputLogger::closeFile() {
    if (fileOpen) {
        if (format == OutputFormat::JSON && !logEntries.empty()) {
            file << "]" << std::endl;
        }
        file.close();
        fileOpen = false;
    }
}

void OutputLogger::flush() {
    if (fileOpen) {
        file.flush();
    }
}

void OutputLogger::clear() {
    logEntries.clear();
    vcdHeaderWritten = false;
}

void OutputLogger::printStatistics() const {
    if (logEntries.empty()) {
        std::cout << "No log entries to analyze" << std::endl;
        return;
    }
    
    std::cout << "=== Simulation Statistics ===" << std::endl;
    std::cout << "Total cycles: " << getTotalCycles() << std::endl;
    std::cout << "Active cycles: " << getActiveCycles() << std::endl;
    std::cout << "Average state activity: " << std::fixed << std::setprecision(2) 
              << (getAverageStateActivity() * 100) << "%" << std::endl;
    
    // Count state transitions
    auto transitions = getStateTransitionCycles();
    std::cout << "State transitions: " << transitions.size() << std::endl;
    
    // Count address changes
    auto addrChanges = getAddressTransitions();
    std::cout << "Address changes: " << addrChanges.size() << std::endl;
    
    std::cout << "=============================" << std::endl;
}

uint32_t OutputLogger::getTotalCycles() const {
    if (logEntries.empty()) return 0;
    return logEntries.back().cycle + 1;
}

uint32_t OutputLogger::getActiveCycles() const {
    uint32_t count = 0;
    for (const auto& entry : logEntries) {
        if (entry.ready || entry.fired) {
            count++;
        }
    }
    return count;
}

double OutputLogger::getAverageStateActivity() const {
    if (logEntries.empty()) return 0.0;
    
    double totalActivity = 0.0;
    for (const auto& entry : logEntries) {
        uint32_t activeStates = 0;
        for (bool state : entry.states) {
            if (state) activeStates++;
        }
        totalActivity += static_cast<double>(activeStates) / entry.states.size();
    }
    
    return totalActivity / logEntries.size();
}

std::vector<uint32_t> OutputLogger::getStateTransitionCycles() const {
    std::vector<uint32_t> transitions;
    
    if (logEntries.size() < 2) return transitions;
    
    for (size_t i = 1; i < logEntries.size(); ++i) {
        if (logEntries[i].states != logEntries[i-1].states) {
            transitions.push_back(logEntries[i].cycle);
        }
    }
    
    return transitions;
}

std::vector<uint32_t> OutputLogger::getAddressTransitions() const {
    std::vector<uint32_t> transitions;
    
    if (logEntries.size() < 2) return transitions;
    
    for (size_t i = 1; i < logEntries.size(); ++i) {
        if (logEntries[i].address != logEntries[i-1].address) {
            transitions.push_back(logEntries[i].cycle);
        }
    }
    
    return transitions;
}

std::unique_ptr<OutputLogger> OutputLogger::createConsoleLogger() {
    auto logger = std::make_unique<OutputLogger>();
    logger->setFormat(OutputFormat::CONSOLE);
    logger->setRealTime(true);
    return logger;
}

std::unique_ptr<OutputLogger> OutputLogger::createVCDLogger(const std::string& filename) {
    auto logger = std::make_unique<OutputLogger>();
    logger->setFormat(OutputFormat::VCD);
    logger->setFilename(filename);
    logger->setRealTime(false);
    return logger;
}

std::unique_ptr<OutputLogger> OutputLogger::createCSVLogger(const std::string& filename) {
    auto logger = std::make_unique<OutputLogger>();
    logger->setFormat(OutputFormat::CSV);
    logger->setFilename(filename);
    logger->setRealTime(false);
    return logger;
}

std::unique_ptr<OutputLogger> OutputLogger::createJSONLogger(const std::string& filename) {
    auto logger = std::make_unique<OutputLogger>();
    logger->setFormat(OutputFormat::JSON);
    logger->setFilename(filename);
    logger->setRealTime(false);
    return logger;
}

bool OutputLogger::exportToFile(const std::string& exportFilename, OutputFormat exportFormat) {
    if (logEntries.empty()) {
        std::cerr << "No log entries to export" << std::endl;
        return false;
    }
    
    std::ofstream exportFile(exportFilename);
    if (!exportFile.is_open()) {
        std::cerr << "Failed to open export file: " << exportFilename << std::endl;
        return false;
    }
    
    // Write header based on format
    if (exportFormat == OutputFormat::CSV) {
        exportFile << "Cycle,Address,Ready,LHS,Fired";
        // Add state columns
        if (!logEntries.empty()) {
            for (size_t i = 0; i < logEntries[0].states.size(); ++i) {
                exportFile << ",State" << i;
            }
            for (size_t i = 0; i < logEntries[0].outputs.size(); ++i) {
                exportFile << ",Output" << i;
            }
            for (size_t i = 0; i < logEntries[0].inputs.size(); ++i) {
                exportFile << ",Input" << i;
            }
        }
        exportFile << std::endl;
    } else if (exportFormat == OutputFormat::JSON) {
        exportFile << "[" << std::endl;
    }
    
    // Write data entries
    for (size_t i = 0; i < logEntries.size(); ++i) {
        const LogEntry& entry = logEntries[i];
        
        if (exportFormat == OutputFormat::CSV) {
            exportFile << entry.cycle << "," << entry.address << ","
                      << (entry.ready ? "1" : "0") << "," << (entry.lhs ? "1" : "0") << ","
                      << (entry.fired ? "1" : "0");
            
            // Write states
            for (bool state : entry.states) {
                exportFile << "," << (state ? "1" : "0");
            }
            
            // Write outputs
            for (uint8_t output : entry.outputs) {
                exportFile << ",0x" << std::hex << static_cast<uint32_t>(output) << std::dec;
            }
            
            // Write inputs
            for (uint8_t input : entry.inputs) {
                exportFile << ",0x" << std::hex << static_cast<uint32_t>(input) << std::dec;
            }
            
            exportFile << std::endl;
            
        } else if (exportFormat == OutputFormat::JSON) {
            exportFile << "  {" << std::endl;
            exportFile << "    \"cycle\": " << entry.cycle << "," << std::endl;
            exportFile << "    \"address\": " << entry.address << "," << std::endl;
            exportFile << "    \"ready\": " << (entry.ready ? "true" : "false") << "," << std::endl;
            exportFile << "    \"lhs\": " << (entry.lhs ? "true" : "false") << "," << std::endl;
            exportFile << "    \"fired\": " << (entry.fired ? "true" : "false") << "," << std::endl;
            
            // Write states
            exportFile << "    \"states\": [";
            for (size_t j = 0; j < entry.states.size(); ++j) {
                if (j > 0) exportFile << ", ";
                exportFile << (entry.states[j] ? "true" : "false");
            }
            exportFile << "]," << std::endl;
            
            // Write outputs
            exportFile << "    \"outputs\": [";
            for (size_t j = 0; j < entry.outputs.size(); ++j) {
                if (j > 0) exportFile << ", ";
                exportFile << static_cast<uint32_t>(entry.outputs[j]);
            }
            exportFile << "]," << std::endl;
            
            // Write inputs
            exportFile << "    \"inputs\": [";
            for (size_t j = 0; j < entry.inputs.size(); ++j) {
                if (j > 0) exportFile << ", ";
                exportFile << static_cast<uint32_t>(entry.inputs[j]);
            }
            exportFile << "]" << std::endl;
            
            exportFile << "  }" << (i < logEntries.size() - 1 ? "," : "") << std::endl;
        }
    }
    
    if (exportFormat == OutputFormat::JSON) {
        exportFile << "]" << std::endl;
    }
    
    exportFile.close();
    std::cout << "Exported " << logEntries.size() << " entries to " << exportFilename << std::endl;
    return true;
}

} // namespace HotstateSim
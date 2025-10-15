#include "simulator.h"
#include "utils.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>

namespace HotstateSim {

Simulator::Simulator(const SimulatorConfig& cfg)
    : config(cfg)
    , state(SimulatorState::IDLE)
    , currentCycle(0)
    , cyclesSinceStart(0)
    , breakpointHit(false)
{
    stimulus = std::make_unique<StimulusParser>();
}

bool Simulator::initialize() {
    state = SimulatorState::LOADING;
    
    try {
        // Validate configuration
        if (!validateConfiguration()) {
            lastError = "Invalid configuration";
            state = SimulatorState::ERROR;
            return false;
        }
        
        // Load memory files
        if (!loadMemoryFiles()) {
            state = SimulatorState::ERROR;
            return false;
        }
        
        // Load stimulus file
        if (!config.stimulusFile.empty()) {
            if (!loadStimulusFile()) {
                state = SimulatorState::ERROR;
                return false;
            }
        }
        
        // Initialize logger
        if (!initializeLogger()) {
            state = SimulatorState::ERROR;
            return false;
        }
        
        // Initialize hotstate model
        initializeHotstate();
        
        // Reset simulation state
        currentCycle = 0;
        cyclesSinceStart = 0;
        breakpointHit = false;
        
        state = SimulatorState::READY;
        
        if (config.verbose) {
            std::cout << "Simulator initialized successfully" << std::endl;
            std::cout << "Base path: " << config.basePath << std::endl;
            std::cout << "Max cycles: " << config.maxCycles << std::endl;
            if (!config.stimulusFile.empty()) {
                std::cout << "Stimulus file: " << config.stimulusFile << std::endl;
            }
        }
        
        return true;
        
    } catch (const SimulatorException& e) {
        lastError = e.what();
        state = SimulatorState::ERROR;
        return false;
    }
}

bool Simulator::run() {
    if (state != SimulatorState::READY && state != SimulatorState::PAUSED) {
        lastError = "Simulator not ready to run";
        return false;
    }
    
    state = SimulatorState::RUNNING;
    
    if (config.verbose) {
        std::cout << "Starting simulation..." << std::endl;
    }
    
    try {
        while (state == SimulatorState::RUNNING && currentCycle < config.maxCycles) {
            // Check breakpoints
            if (config.enableBreakpoints) {
                checkBreakpoints();
                if (breakpointHit) {
                    state = SimulatorState::PAUSED;
                    if (config.verbose) {
                        std::cout << "Breakpoint hit at cycle " << currentCycle << std::endl;
                    }
                    break;
                }
            }
            
            // Apply stimulus for current cycle
            applyStimulus(currentCycle);
            
            // Advance clock
            advanceClock();
            
            // Log the cycle
            if (logger) {
                std::vector<uint8_t> inputs = stimulus->getInputs(currentCycle);
                logger->logCycle(currentCycle, *hotstate, inputs);
            }
            
            // Debug output
            if (config.debugMode) {
                printDebugInfo(currentCycle);
            } else if (config.verbose && (currentCycle % 100 == 0)) {
                std::cout << "Cycle: " << currentCycle << std::endl;
            }
            
            currentCycle++;
            cyclesSinceStart++;
        }
        
        if (currentCycle >= config.maxCycles) {
            state = SimulatorState::FINISHED;
            if (config.verbose) {
                std::cout << "Simulation completed: Maximum cycles reached" << std::endl;
            }
        }
        
        return true;
        
    } catch (const SimulatorException& e) {
        lastError = e.what();
        state = SimulatorState::ERROR;
        return false;
    }
}

bool Simulator::runToCompletion() {
    if (!initialize()) {
        return false;
    }
    
    bool result = run();
    
    if (config.verbose) {
        printStatistics();
    }
    
    return result;
}

bool Simulator::step(uint32_t numCycles) {
    if (state != SimulatorState::READY && state != SimulatorState::PAUSED) {
        lastError = "Simulator not ready to step";
        return false;
    }
    
    state = SimulatorState::RUNNING;
    
    try {
        for (uint32_t i = 0; i < numCycles && currentCycle < config.maxCycles; ++i) {
            applyStimulus(currentCycle);
            advanceClock();
            
            if (logger) {
                std::vector<uint8_t> inputs = stimulus->getInputs(currentCycle);
                logger->logCycle(currentCycle, *hotstate, inputs);
            }
            
            if (config.debugMode) {
                printDebugInfo(currentCycle);
            }
            
            currentCycle++;
            cyclesSinceStart++;
        }
        
        state = SimulatorState::PAUSED;
        return true;
        
    } catch (const SimulatorException& e) {
        lastError = e.what();
        state = SimulatorState::ERROR;
        return false;
    }
}

void Simulator::pause() {
    if (state == SimulatorState::RUNNING) {
        state = SimulatorState::PAUSED;
        if (config.verbose) {
            std::cout << "Simulation paused at cycle " << currentCycle << std::endl;
        }
    }
}

void Simulator::reset() {
    if (hotstate) {
        hotstate->reset();
    }
    
    currentCycle = 0;
    cyclesSinceStart = 0;
    breakpointHit = false;
    
    if (logger) {
        logger->clear();
    }
    
    state = SimulatorState::READY;
    
    if (config.verbose) {
        std::cout << "Simulation reset" << std::endl;
    }
}

void Simulator::stop() {
    state = SimulatorState::FINISHED;
    if (config.verbose) {
        std::cout << "Simulation stopped at cycle " << currentCycle << std::endl;
    }
}

double Simulator::getProgress() const {
    if (config.maxCycles == 0) return 0.0;
    return static_cast<double>(currentCycle) / config.maxCycles;
}

bool Simulator::loadMemoryFiles() {
    if (config.basePath.empty()) {
        lastError = "Base path not specified";
        return false;
    }
    
    if (!memoryLoader.loadFromBasePath(config.basePath)) {
        lastError = "Failed to load memory files from base path: " + config.basePath;
        return false;
    }
    
    if (!memoryLoader.isLoaded()) {
        lastError = "Memory files not properly loaded";
        return false;
    }
    
    if (config.verbose) {
        memoryLoader.printMemoryInfo();
    }
    
    return true;
}

bool Simulator::loadStimulusFile() {
    if (!stimulus->loadStimulus(config.stimulusFile)) {
        lastError = "Failed to load stimulus file: " + config.stimulusFile;
        return false;
    }
    
    if (config.verbose) {
        std::cout << "Loaded " << stimulus->size() << " stimulus entries" << std::endl;
    }
    
    return true;
}

bool Simulator::initializeLogger() {
    switch (config.outputFormat) {
        case OutputFormat::CONSOLE:
            logger = OutputLogger::createConsoleLogger();
            break;
        case OutputFormat::VCD:
            logger = OutputLogger::createVCDLogger(config.outputFile);
            break;
        case OutputFormat::CSV:
            logger = OutputLogger::createCSVLogger(config.outputFile);
            break;
        case OutputFormat::JSON:
            logger = OutputLogger::createJSONLogger(config.outputFile);
            break;
    }
    
    if (!logger) {
        lastError = "Failed to create logger";
        return false;
    }
    
    logger->setRealTime(config.realTimeOutput);
    
    // Open file if needed
    if (config.outputFormat != OutputFormat::CONSOLE && !config.outputFile.empty()) {
        if (!logger->openFile()) {
            lastError = "Failed to open output file: " + config.outputFile;
            return false;
        }
    }
    
    return true;
}

void Simulator::initializeHotstate() {
    hotstate = std::make_unique<HotstateModel>(memoryLoader);
    hotstate->reset();
}

void Simulator::advanceClock() {
    if (hotstate) {
        hotstate->clock();
    }
}

void Simulator::applyStimulus(uint32_t cycle) {
    if (stimulus && !stimulus->isEmpty()) {
        std::vector<uint8_t> inputs = stimulus->getInputs(cycle);
        hotstate->setInputs(inputs);
    }
}

void Simulator::checkBreakpoints() {
    breakpointHit = false;
    
    if (!hotstate) return;
    
    // Check state breakpoints
    for (uint32_t stateValue : config.breakpointStates) {
        auto states = hotstate->getStates();
        if (stateValue < states.size() && states[stateValue]) {
            breakpointHit = true;
            if (config.debugMode) {
                std::cout << "State breakpoint hit: state[" << stateValue << "] = 1" << std::endl;
            }
            break;
        }
    }
    
    // Check address breakpoints
    if (!breakpointHit) {
        uint32_t currentAddr = hotstate->getCurrentAddress();
        for (uint32_t addr : config.breakpointAddresses) {
            if (currentAddr == addr) {
                breakpointHit = true;
                if (config.debugMode) {
                    std::cout << "Address breakpoint hit: address = 0x" << std::hex << addr << std::dec << std::endl;
                }
                break;
            }
        }
    }
}

void Simulator::printDebugInfo(uint32_t cycle) {
    std::cout << "=== Debug Info - Cycle " << cycle << " ===" << std::endl;
    if (hotstate) {
        hotstate->printState();
        hotstate->printControlSignals();
    }
    std::cout << "=================================" << std::endl;
}

void Simulator::printCycleInfo(uint32_t cycle) {
    std::cout << "Cycle: " << std::setw(6) << cycle;
    if (hotstate) {
        std::cout << ", Addr: 0x" << std::hex << std::setw(4) << hotstate->getCurrentAddress() << std::dec;
        std::cout << ", Ready: " << (hotstate->isReady() ? "1" : "0");
    }
    std::cout << std::endl;
}

bool Simulator::validateConfiguration() {
    if (config.basePath.empty()) {
        lastError = "Base path is required";
        return false;
    }
    
    if (config.maxCycles == 0) {
        lastError = "Max cycles must be greater than 0";
        return false;
    }
    
    if (config.outputFormat != OutputFormat::CONSOLE && config.outputFile.empty()) {
        lastError = "Output file is required for non-console output formats";
        return false;
    }
    
    return true;
}

void Simulator::printStatistics() const {
    std::cout << "=== Simulation Statistics ===" << std::endl;
    std::cout << "Total cycles simulated: " << cyclesSinceStart << std::endl;
    std::cout << "Final state: " << stateToString(state) << std::endl;
    
    if (logger) {
        logger->printStatistics();
    }
    
    if (hotstate) {
        std::cout << "Final address: 0x" << std::hex << hotstate->getCurrentAddress() << std::dec << std::endl;
        std::cout << "Final ready state: " << (hotstate->isReady() ? "1" : "0") << std::endl;
    }
    
    std::cout << "=============================" << std::endl;
}

void Simulator::printSummary() const {
    std::cout << "=== Simulation Summary ===" << std::endl;
    std::cout << "Base path: " << config.basePath << std::endl;
    std::cout << "Cycles simulated: " << cyclesSinceStart << " / " << config.maxCycles << std::endl;
    std::cout << "Progress: " << std::fixed << std::setprecision(1) << (getProgress() * 100) << "%" << std::endl;
    std::cout << "State: " << stateToString(state) << std::endl;
    
    if (hasError()) {
        std::cout << "Error: " << lastError << std::endl;
    }
    
    std::cout << "=========================" << std::endl;
}

void Simulator::printCurrentState() const {
    if (hotstate) {
        hotstate->printState();
    }
}

void Simulator::printMemoryInfo() const {
    memoryLoader.printMemoryInfo();
}

void Simulator::addStateBreakpoint(uint32_t stateValue) {
    config.breakpointStates.push_back(stateValue);
    config.enableBreakpoints = true;
}

void Simulator::addAddressBreakpoint(uint32_t address) {
    config.breakpointAddresses.push_back(address);
    config.enableBreakpoints = true;
}

void Simulator::clearBreakpoints() {
    config.breakpointStates.clear();
    config.breakpointAddresses.clear();
    config.enableBreakpoints = false;
}

void Simulator::listBreakpoints() const {
    std::cout << "=== Breakpoints ===" << std::endl;
    
    if (config.breakpointStates.empty() && config.breakpointAddresses.empty()) {
        std::cout << "No breakpoints set" << std::endl;
    } else {
        if (!config.breakpointStates.empty()) {
            std::cout << "State breakpoints: ";
            for (uint32_t state : config.breakpointStates) {
                std::cout << state << " ";
            }
            std::cout << std::endl;
        }
        
        if (!config.breakpointAddresses.empty()) {
            std::cout << "Address breakpoints: ";
            for (uint32_t addr : config.breakpointAddresses) {
                std::cout << "0x" << std::hex << addr << std::dec << " ";
            }
            std::cout << std::endl;
        }
    }
    
    std::cout << "===================" << std::endl;
}

bool Simulator::exportResults(const std::string& filename, OutputFormat format) {
    if (!logger) {
        lastError = "No logger available for export";
        return false;
    }
    
    return logger->exportToFile(filename, format);
}

bool Simulator::exportTrace(const std::string& filename) {
    return exportResults(filename, OutputFormat::CSV);
}

bool Simulator::exportSummary(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        lastError = "Failed to open summary file: " + filename;
        return false;
    }
    
    file << "Simulation Summary\n";
    file << "==================\n";
    file << "Base path: " << config.basePath << "\n";
    file << "Cycles simulated: " << cyclesSinceStart << "\n";
    file << "Max cycles: " << config.maxCycles << "\n";
    file << "Progress: " << (getProgress() * 100) << "%\n";
    file << "Final state: " << stateToString(state) << "\n";
    
    if (hasError()) {
        file << "Error: " << lastError << "\n";
    }
    
    if (hotstate) {
        file << "Final address: 0x" << std::hex << hotstate->getCurrentAddress() << std::dec << "\n";
    }
    
    file.close();
    return true;
}

std::string stateToString(SimulatorState state) {
    switch (state) {
        case SimulatorState::IDLE: return "IDLE";
        case SimulatorState::LOADING: return "LOADING";
        case SimulatorState::READY: return "READY";
        case SimulatorState::RUNNING: return "RUNNING";
        case SimulatorState::PAUSED: return "PAUSED";
        case SimulatorState::FINISHED: return "FINISHED";
        case SimulatorState::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

SimulatorConfig Simulator::createDefaultConfig() {
    return SimulatorConfig{};
}

SimulatorConfig Simulator::createDebugConfig(const std::string& basePath) {
    SimulatorConfig config;
    config.basePath = basePath;
    config.debugMode = true;
    config.verbose = true;
    config.realTimeOutput = true;
    config.outputFormat = OutputFormat::CONSOLE;
    return config;
}

SimulatorConfig Simulator::createBatchConfig(const std::string& basePath, 
                                            const std::string& outputFile) {
    SimulatorConfig config;
    config.basePath = basePath;
    config.outputFile = outputFile;
    config.outputFormat = OutputFormat::CSV;
    config.verbose = false;
    config.realTimeOutput = false;
    return config;
}

} // namespace HotstateSim
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
    , debugMode(false)
    , debugPaused(false)
    , breakpointReason("")
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

        // Enter debug mode if requested
        if (config.debugMode) {
            enterDebugMode();
        }

        if (config.verbose) {
            std::cout << "Simulator initialized successfully" << std::endl;
            std::cout << "Base path: " << config.basePath << std::endl;
            std::cout << "Max cycles: " << config.maxCycles << std::endl;
            if (!config.stimulusFile.empty()) {
                std::cout << "Stimulus file: " << config.stimulusFile << std::endl;
            }
            if (config.debugMode) {
                std::cout << "Debug mode: enabled" << std::endl;
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
    breakpointReason = "";
    debugPaused = false;

    // Clear debugger state
    watchVariables.clear();
    watchStates.clear();
    manualInputs.clear();

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
            breakpointReason = "State[" + std::to_string(stateValue) + "] = 1";
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
                breakpointReason = "Address = 0x" + toHexString(addr);
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

// === DEBUGGER IMPLEMENTATION ===

void Simulator::enterDebugMode() {
    debugMode = true;
    debugPaused = true;
    if (config.verbose) {
        std::cout << "Entered debug mode" << std::endl;
    }
}

void Simulator::exitDebugMode() {
    debugMode = false;
    debugPaused = false;
    if (config.verbose) {
        std::cout << "Exited debug mode" << std::endl;
    }
}

bool Simulator::debugStep() {
    if (!hotstate || !debugMode) {
        return false;
    }

    if (currentCycle >= config.maxCycles) {
        return false;
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

    currentCycle++;
    cyclesSinceStart++;

    // Print debug info
    printDebugInfo(currentCycle - 1);

    return true;
}

void Simulator::debugContinue() {
    debugPaused = false;
    if (config.verbose) {
        std::cout << "Continuing simulation..." << std::endl;
    }
}

void Simulator::debugPause() {
    debugPaused = true;
    if (config.verbose) {
        std::cout << "Simulation paused" << std::endl;
    }
}

void Simulator::inspectState() const {
    if (!hotstate) return;

    std::cout << "=== State Inspection ===" << std::endl;
    std::cout << "Current Address: 0x" << std::hex << hotstate->getCurrentAddress() << std::dec << std::endl;
    std::cout << "States: ";
    auto states = hotstate->getStates();
    for (size_t i = 0; i < states.size(); ++i) {
        std::cout << states[i];
    }
    std::cout << std::endl;
    std::cout << "Ready: " << (hotstate->isReady() ? "1" : "0") << std::endl;
    std::cout << "========================" << std::endl;
}

void Simulator::inspectVariables() const {
    if (!hotstate) return;

    std::cout << "=== Variable Inspection ===" << std::endl;
    auto outputs = hotstate->getOutputs();
    for (size_t i = 0; i < outputs.size(); ++i) {
        std::cout << "Output[" << i << "] = 0x" << std::hex << static_cast<uint32_t>(outputs[i]) << std::dec << std::endl;
    }
    std::cout << "===========================" << std::endl;
}

void Simulator::inspectMicrocode() const {
    if (!hotstate) return;

    std::cout << "=== Microcode Inspection ===" << std::endl;
    uint32_t addr = hotstate->getCurrentAddress();
    std::cout << "Current Address: 0x" << std::hex << addr << std::dec << std::endl;

    if (addr < memoryLoader.getSmdata().size()) {
        uint64_t microcode = memoryLoader.getSmdata()[addr];
        std::cout << "Microcode: 0x" << std::hex << microcode << std::dec << std::endl;
    }

    hotstate->printMicrocode();
    std::cout << "============================" << std::endl;
}

void Simulator::inspectMemory(uint32_t startAddr, uint32_t count) const {
    std::cout << "=== Memory Inspection ===" << std::endl;
    std::cout << "Vardata:" << std::endl;
    auto vardata = memoryLoader.getVardata();
    for (uint32_t i = startAddr; i < std::min(startAddr + count, static_cast<uint32_t>(vardata.size())); ++i) {
        std::cout << "  [" << i << "] = 0x" << std::hex << vardata[i] << std::dec << std::endl;
    }

    std::cout << "Switchdata:" << std::endl;
    auto switchdata = memoryLoader.getSwitchdata();
    for (uint32_t i = startAddr; i < std::min(startAddr + count, static_cast<uint32_t>(switchdata.size())); ++i) {
        std::cout << "  [" << i << "] = " << switchdata[i] << std::endl;
    }
    std::cout << "=========================" << std::endl;
}

void Simulator::inspectStack() const {
    if (!hotstate) return;

    std::cout << "=== Stack Inspection ===" << std::endl;
    hotstate->printStack();
    std::cout << "========================" << std::endl;
}

void Simulator::inspectControlSignals() const {
    if (!hotstate) return;

    std::cout << "=== Control Signals ===" << std::endl;
    hotstate->printControlSignals();
    std::cout << "========================" << std::endl;
}

void Simulator::inspectInputs() const {
    if (!hotstate) return;

    std::cout << "=== Input Inspection ===" << std::endl;
    std::cout << "Current Inputs: [";
    for (size_t i = 0; i < manualInputs.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << "0x" << std::hex << static_cast<uint32_t>(manualInputs[i]) << std::dec;
    }
    std::cout << "]" << std::endl;

    // Show stimulus inputs if available
    if (stimulus && !stimulus->isEmpty()) {
        std::vector<uint8_t> stimulusInputs = stimulus->getInputs(currentCycle);
        std::cout << "Stimulus Inputs: [";
        for (size_t i = 0; i < stimulusInputs.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << "0x" << std::hex << static_cast<uint32_t>(stimulusInputs[i]) << std::dec;
        }
        std::cout << "]" << std::endl;
    }

    std::cout << "=========================" << std::endl;
}

void Simulator::setInputValue(uint32_t inputIndex, uint8_t value) {
    if (inputIndex >= manualInputs.size()) {
        manualInputs.resize(inputIndex + 1, 0);
    }
    manualInputs[inputIndex] = value;

    if (hotstate) {
        std::vector<uint8_t> inputs = manualInputs;
        hotstate->setInputs(inputs);
    }

    if (config.verbose) {
        std::cout << "Set input[" << inputIndex << "] = 0x" << std::hex << static_cast<uint32_t>(value) << std::dec << std::endl;
    }
}

void Simulator::setVariableValue(uint32_t varIndex, uint8_t value) {
    if (hotstate) {
        auto outputs = hotstate->getOutputs();
        if (varIndex < outputs.size()) {
            std::vector<uint8_t> newOutputs = outputs;
            newOutputs[varIndex] = value;
            hotstate->setInputs(newOutputs); // This will be treated as variable inputs
        }
    }

    if (config.verbose) {
        std::cout << "Set variable[" << varIndex << "] = 0x" << std::hex << static_cast<uint32_t>(value) << std::dec << std::endl;
    }
}

bool Simulator::setInputValueByName(const std::string& name, uint8_t value) {
    if (memoryLoader.hasSymbolTable()) {
        uint32_t index = memoryLoader.getInputIndexByName(name);
        if (index != UINT32_MAX) {
            setInputValue(index, value);
            return true;
        }
    }
    std::cout << "Error: Input variable '" << name << "' not found in symbol table" << std::endl;
    return false;
}

bool Simulator::setVariableValueByName(const std::string& name, uint8_t value) {
    if (memoryLoader.hasSymbolTable()) {
        uint32_t index = memoryLoader.getStateIndexByName(name);
        if (index != UINT32_MAX) {
            setVariableValue(index, value);
            return true;
        }
    }
    std::cout << "Error: State variable '" << name << "' not found in symbol table" << std::endl;
    return false;
}

void Simulator::addWatchVariable(uint32_t varIndex) {
    watchVariables.push_back(varIndex);
    if (config.verbose) {
        std::cout << "Added watch for variable[" << varIndex << "]" << std::endl;
    }
}

void Simulator::addWatchState(uint32_t stateIndex) {
    watchStates.push_back(stateIndex);
    if (config.verbose) {
        std::cout << "Added watch for state[" << stateIndex << "]" << std::endl;
    }
}

void Simulator::removeWatch(uint32_t watchIndex) {
    if (watchIndex < watchVariables.size()) {
        watchVariables.erase(watchVariables.begin() + watchIndex);
    } else if (watchIndex < watchVariables.size() + watchStates.size()) {
        watchStates.erase(watchStates.begin() + (watchIndex - watchVariables.size()));
    }
}

void Simulator::clearWatches() {
    watchVariables.clear();
    watchStates.clear();
    if (config.verbose) {
        std::cout << "Cleared all watches" << std::endl;
    }
}

void Simulator::listWatches() const {
    std::cout << "=== Watch List ===" << std::endl;
    for (size_t i = 0; i < watchVariables.size(); ++i) {
        std::cout << "Watch " << i << ": Variable[" << watchVariables[i] << "]" << std::endl;
    }
    for (size_t i = 0; i < watchStates.size(); ++i) {
        std::cout << "Watch " << (i + watchVariables.size()) << ": State[" << watchStates[i] << "]" << std::endl;
    }
    std::cout << "==================" << std::endl;
}

void Simulator::evaluateWatches() const {
    if (!hotstate) return;

    std::cout << "=== Watch Evaluation ===" << std::endl;

    auto outputs = hotstate->getOutputs();
    for (size_t i = 0; i < watchVariables.size(); ++i) {
        uint32_t varIndex = watchVariables[i];
        if (varIndex < outputs.size()) {
            std::cout << "Variable[" << varIndex << "] = 0x" << std::hex << static_cast<uint32_t>(outputs[varIndex]) << std::dec << std::endl;
        }
    }

    auto states = hotstate->getStates();
    for (size_t i = 0; i < watchStates.size(); ++i) {
        uint32_t stateIndex = watchStates[i];
        if (stateIndex < states.size()) {
            std::cout << "State[" << stateIndex << "] = " << states[stateIndex] << std::endl;
        }
    }

    std::cout << "========================" << std::endl;
}

void Simulator::printCurrentInstruction() const {
    if (!hotstate) return;

    std::cout << "=== Current Instruction ===" << std::endl;
    uint32_t addr = hotstate->getCurrentAddress();
    std::cout << "Address: 0x" << std::hex << addr << std::dec << std::endl;

    if (addr < memoryLoader.getSmdata().size()) {
        uint64_t microcode = memoryLoader.getSmdata()[addr];
        std::cout << "Microcode: 0x" << std::hex << microcode << std::dec << std::endl;

        // Decode the microcode fields
        std::cout << "Decoded fields:" << std::endl;

        // Extract basic fields (this is simplified - full decoding would need the parameter widths)
        uint32_t stateValue = microcode & 0xFF;
        uint32_t transitionValue = (microcode >> 8) & 0xFF;
        uint32_t jadr = (microcode >> 16) & 0xFF;

        std::cout << "  State Value: 0x" << std::hex << stateValue << std::dec << std::endl;
        std::cout << "  Transition Value: 0x" << std::hex << transitionValue << std::dec << std::endl;
        std::cout << "  Jump Address: 0x" << std::hex << jadr << std::dec << std::endl;
    }

    std::cout << "==========================" << std::endl;
}

void Simulator::printMicrocodeAt(uint32_t address) const {
    std::cout << "=== Microcode at Address 0x" << std::hex << address << std::dec << " ===" << std::endl;

    if (address < memoryLoader.getSmdata().size()) {
        uint64_t microcode = memoryLoader.getSmdata()[address];
        std::cout << "Microcode: 0x" << std::hex << microcode << std::dec << std::endl;
    } else {
        std::cout << "Address out of range" << std::endl;
    }

    std::cout << "================================" << std::endl;
}

std::string Simulator::getBreakpointReason() const {
    return breakpointReason;
}

std::string toHexString(uint32_t value) {
    std::stringstream ss;
    ss << "0x" << std::hex << value;
    return ss.str();
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
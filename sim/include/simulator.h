#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "memory_loader.h"
#include "hotstate_model.h"
#include "stimulus_parser.h"
#include "output_logger.h"
#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace HotstateSim {

enum class SimulatorState {
    IDLE,
    LOADING,
    READY,
    RUNNING,
    PAUSED,
    FINISHED,
    ERROR
};

struct SimulatorConfig {
    std::string basePath;
    std::string stimulusFile;
    std::string outputFile;
    OutputFormat outputFormat;
    uint32_t maxCycles;
    bool debugMode;
    bool verbose;
    bool realTimeOutput;
    bool enableBreakpoints;
    std::vector<uint32_t> breakpointStates;
    std::vector<uint32_t> breakpointAddresses;
    uint32_t cycleStep;
    
    SimulatorConfig() 
        : outputFormat(OutputFormat::CONSOLE)
        , maxCycles(1000)
        , debugMode(false)
        , verbose(false)
        , realTimeOutput(true)
        , enableBreakpoints(false)
        , cycleStep(1)
    {}
};

class Simulator {
private:
    SimulatorConfig config;
    SimulatorState state;
    MemoryLoader memoryLoader;
    std::unique_ptr<HotstateModel> hotstate;
    std::unique_ptr<StimulusParser> stimulus;
    std::unique_ptr<OutputLogger> logger;
    
    uint32_t currentCycle;
    uint32_t cyclesSinceStart;
    bool breakpointHit;
    std::string lastError;
    std::string breakpointReason;

    // Debugger state
    bool debugMode;
    bool debugPaused;
    std::vector<uint32_t> watchVariables;
    std::vector<uint32_t> watchStates;
    std::vector<uint8_t> manualInputs;
    
    // Internal methods
    bool loadMemoryFiles();
    bool loadStimulusFile();
    bool initializeLogger();
    void initializeHotstate();
    void advanceClock();
    void applyStimulus(uint32_t cycle);
    void checkBreakpoints();
    void updateState();
    
    // Debug and analysis
    void printDebugInfo(uint32_t cycle);
    void printCycleInfo(uint32_t cycle);
    bool validateConfiguration();
    
public:
    explicit Simulator(const SimulatorConfig& cfg = SimulatorConfig{});
    ~Simulator() = default;
    
    // Configuration
    void setConfig(const SimulatorConfig& cfg) { config = cfg; }
    const SimulatorConfig& getConfig() const { return config; }
    
    // Simulation control
    bool initialize();
    bool run();
    bool runToCompletion();
    bool step(uint32_t numCycles = 1);
    bool stepToCycle(uint32_t targetCycle);
    void pause();
    void reset();
    void stop();
    
    // Status
    SimulatorState getState() const { return state; }
    bool isRunning() const { return state == SimulatorState::RUNNING; }
    bool isFinished() const { return state == SimulatorState::FINISHED; }
    bool hasError() const { return state == SimulatorState::ERROR; }
    const std::string& getLastError() const { return lastError; }
    
    // Progress
    uint32_t getCurrentCycle() const { return currentCycle; }
    uint32_t getCyclesSinceStart() const { return cyclesSinceStart; }
    double getProgress() const;
    
    // Access to components
    const MemoryLoader& getMemoryLoader() const { return memoryLoader; }
    const HotstateModel& getHotstateModel() const { return *hotstate; }
    const StimulusParser& getStimulusParser() const { return *stimulus; }
    const OutputLogger& getLogger() const { return *logger; }
    
    // Analysis
    void printStatistics() const;
    void printSummary() const;
    void printCurrentState() const;
    void printMemoryInfo() const;
    
    // Breakpoints
    void addStateBreakpoint(uint32_t stateValue);
    void addAddressBreakpoint(uint32_t address);
    void clearBreakpoints();
    void listBreakpoints() const;

    // Debugger features
    void enterDebugMode();
    void exitDebugMode();
    bool isInDebugMode() const { return debugMode; }

    // Single-step debugging
    bool debugStep();
    void debugContinue();
    void debugPause();

    // State inspection
    void inspectState() const;
    void inspectVariables() const;
    void inspectMicrocode() const;
    void inspectMemory(uint32_t startAddr = 0, uint32_t count = 16) const;
    void inspectStack() const;
    void inspectControlSignals() const;
    void inspectInputs() const;

    // Manual input setting
    void setInputValue(uint32_t inputIndex, uint8_t value);
    void setVariableValue(uint32_t varIndex, uint8_t value);
    bool setInputValueByName(const std::string& name, uint8_t value);
    bool setVariableValueByName(const std::string& name, uint8_t value);

    // Watch functionality
    void addWatchVariable(uint32_t varIndex);
    void addWatchState(uint32_t stateIndex);
    void removeWatch(uint32_t watchIndex);
    void clearWatches();
    void listWatches() const;
    void evaluateWatches() const;

    // Microcode debugging
    void printCurrentInstruction() const;
    void printMicrocodeAt(uint32_t address) const;
    bool isBreakpointHit() const { return breakpointHit; }
    std::string getBreakpointReason() const;
    
    // Export
    bool exportResults(const std::string& filename, OutputFormat format = OutputFormat::CSV);
    bool exportTrace(const std::string& filename);
    bool exportSummary(const std::string& filename);
    
    // Static utility methods
    static SimulatorConfig createDefaultConfig();
    static SimulatorConfig createDebugConfig(const std::string& basePath);
    static SimulatorConfig createBatchConfig(const std::string& basePath,
                                            const std::string& outputFile);
};

// Utility function declarations
std::string stateToString(SimulatorState state);
std::string toHexString(uint32_t value);

} // namespace HotstateSim

#endif // SIMULATOR_H
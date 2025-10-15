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

} // namespace HotstateSim

#endif // SIMULATOR_H
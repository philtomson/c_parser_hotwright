#ifndef OUTPUT_LOGGER_H
#define OUTPUT_LOGGER_H

#include "hotstate_model.h"
#include "stimulus_parser.h"
#include <string>
#include <vector>
#include <fstream>
#include <memory>

namespace HotstateSim {

enum class OutputFormat {
    CONSOLE,
    VCD,
    CSV,
    JSON
};

struct LogEntry {
    uint32_t cycle;
    uint32_t address;
    std::vector<bool> states;
    std::vector<uint8_t> outputs;
    std::vector<uint8_t> inputs;
    bool ready;
    bool lhs;
    bool fired;
    bool jmpadr;
    bool switchActive;
    
    LogEntry() : cycle(0), address(0), ready(false), lhs(false), fired(false), jmpadr(false), switchActive(false) {}
};

class OutputLogger {
private:
    OutputFormat format;
    std::string filename;
    std::ofstream file;
    bool fileOpen;
    std::vector<LogEntry> logEntries;
    bool realTime;
    uint32_t maxLogEntries;
    
    // VCD specific
    bool vcdHeaderWritten;
    std::vector<std::string> vcdSignalNames;
    std::vector<std::string> vcdSignalCodes;
    
    // Helper methods
    void writeConsoleEntry(const LogEntry& entry, const HotstateModel& model);
    void writeVCDEntry(const LogEntry& entry, const HotstateModel& model);
    void writeCSVEntry(const LogEntry& entry, const HotstateModel& model);
    void writeJSONEntry(const LogEntry& entry, const HotstateModel& model);
    
    void writeVCDHeader(const HotstateModel& model);
    void writeVCDValueChange(const std::string& code, const std::string& value);
    void writeVCDTime(uint64_t time);
    
    std::string generateVCDCode(uint32_t index);
    std::string boolToVCDString(bool value);
    std::string uint8ToVCDString(uint8_t value);
    
public:
    OutputLogger();
    ~OutputLogger();
    
    // Configuration
    void setFormat(OutputFormat fmt) { format = fmt; }
    void setFilename(const std::string& fname) { filename = fname; }
    void setRealTime(bool rt) { realTime = rt; }
    void setMaxLogEntries(uint32_t maxEntries) { maxLogEntries = maxEntries; }
    
    // Access methods
    OutputFormat getFormat() const { return format; }
    const std::string& getFilename() const { return filename; }
    bool isRealTime() const { return realTime; }
    size_t getLogSize() const { return logEntries.size(); }
    
    // File operations
    bool openFile();
    void closeFile();
    bool isFileOpen() const { return fileOpen; }
    
    // Logging operations
    void logCycle(uint32_t cycle, const HotstateModel& model, const std::vector<uint8_t>& inputs = {});
    void logEntry(const LogEntry& entry, const HotstateModel& model);
    void logEntry(const LogEntry& entry);
    
    // Batch operations
    void flush();
    void clear();
    
    // Analysis
    std::vector<LogEntry> getEntriesInRange(uint32_t startCycle, uint32_t endCycle) const;
    LogEntry getEntryAtCycle(uint32_t cycle) const;
    std::vector<uint32_t> getStateTransitionCycles() const;
    std::vector<uint32_t> getAddressTransitions() const;
    
    // Statistics
    void printStatistics() const;
    uint32_t getTotalCycles() const;
    uint32_t getActiveCycles() const;
    double getAverageStateActivity() const;
    
    // Export
    bool exportToFile(const std::string& exportFilename, OutputFormat exportFormat);
    bool exportToCSV(const std::string& filename);
    bool exportToJSON(const std::string& filename);
    
    // Debug
    void printLogEntry(uint32_t index) const;
    void printRecentEntries(uint32_t count = 10) const;
    void printSummary() const;
    
    // Static utility methods
    static std::unique_ptr<OutputLogger> createConsoleLogger();
    static std::unique_ptr<OutputLogger> createVCDLogger(const std::string& filename);
    static std::unique_ptr<OutputLogger> createCSVLogger(const std::string& filename);
    static std::unique_ptr<OutputLogger> createJSONLogger(const std::string& filename);
};

} // namespace HotstateSim

#endif // OUTPUT_LOGGER_H
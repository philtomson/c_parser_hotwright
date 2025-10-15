#ifndef HOTSTATE_MODEL_H
#define HOTSTATE_MODEL_H

#include "memory_loader.h"
#include <vector>
#include <cstdint>

namespace HotstateSim {

class HotstateModel {
private:
    // Memory references
    const std::vector<uint32_t>& vardata;
    const std::vector<uint32_t>& switchdata;
    const std::vector<uint64_t>& smdata;
    const Parameters& params;
    
    // State registers
    std::vector<bool> states;
    std::vector<uint8_t> variables;
    uint32_t address;
    uint32_t returnAddress;
    uint32_t stack[16];  // Simple stack implementation
    uint32_t stackPointer;
    
    // Control signals
    bool ready;
    bool lhs;
    bool forcedJmp;
    bool jmpadr;
    bool sub;
    bool rtn;
    bool branch;
    bool stateCapture;
    bool switchActive;
    bool fired;
    bool varOrTimer;
    
    // Microcode fields
    uint32_t jadr;
    uint32_t varSel;
    uint32_t timerSel;
    uint32_t timerLd;
    uint32_t switchSel;
    uint32_t switchAdr;
    std::vector<bool> stateValue;
    std::vector<bool> transitionValue;
    
    // Timing and control
    bool clk;
    bool rst;
    bool hlt;
    uint64_t cycleCount;
    
    // Helper methods
    void executeMicrocode();
    void updateStates();
    void handleControlLogic();
    void handleSwitch();
    void handleVariables();
    void handleNextAddress();
    void extractMicrocodeFields(uint64_t microcode);
    uint32_t calculateSwitchAddress();
    
public:
    HotstateModel(const MemoryLoader& memory);
    
    // Reset and clock
    void reset();
    void clock();
    
    // Input/Output
    void setInputs(const std::vector<uint8_t>& inputs);
    std::vector<uint8_t> getOutputs() const;
    std::vector<bool> getStates() const { return states; }
    uint32_t getCurrentAddress() const { return address; }
    bool isReady() const { return ready; }
    
    // Control
    void setClock(bool clkVal) { clk = clkVal; }
    void setReset(bool rstVal) { rst = rstVal; }
    void setHalt(bool hltVal) { hlt = hltVal; }
    
    // Status
    uint64_t getCycleCount() const { return cycleCount; }
    bool getLhs() const { return lhs; }
    bool getForcedJmp() const { return forcedJmp; }
    bool getJmpadr() const { return jmpadr; }
    bool getSub() const { return sub; }
    bool getRtn() const { return rtn; }
    bool getBranch() const { return branch; }
    bool getStateCapture() const { return stateCapture; }
    bool getSwitchActive() const { return switchActive; }
    bool getFired() const { return fired; }
    bool getVarOrTimer() const { return varOrTimer; }
    
    // Debug
    void printState() const;
    void printMicrocode() const;
    void printControlSignals() const;
    void printVariables() const;
    void printStack() const;
    
    // Validation
    bool validateState() const;
    std::string getStateString() const;
};

} // namespace HotstateSim

#endif // HOTSTATE_MODEL_H
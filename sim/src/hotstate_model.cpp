#include "hotstate_model.h"
#include "utils.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

namespace HotstateSim {

HotstateModel::HotstateModel(const MemoryLoader& memory)
    : vardata(memory.getVardata())
    , switchdata(memory.getSwitchdata())
    , smdata(memory.getSmdata())
    , params(memory.getParams())
    , address(0)
    , returnAddress(0)
    , stackPointer(0)
    , ready(false)
    , lhs(false)
    , forcedJmp(false)
    , jmpadr(false)
    , sub(false)
    , rtn(false)
    , branch(false)
    , stateCapture(false)
    , switchActive(false)
    , fired(false)
    , varOrTimer(false)
    , clk(false)
    , rst(true)
    , hlt(false)
    , cycleCount(0)
    , jadr(0)
    , varSel(0)
    , timerSel(0)
    , timerLd(0)
    , switchSel(0)
    , switchAdr(0)
{
    // Initialize states
    states.resize(params.NUM_STATES, false);
    stateValue.resize(params.NUM_STATES, false);
    transitionValue.resize(params.NUM_STATES, false);
    
    // Initialize variables
    variables.resize(params.NUM_VARS, 0);
    
    // Initialize stack
    std::fill(std::begin(stack), std::end(stack), 0);
    
    std::cout << "HotstateModel initialized with " << params.NUM_STATES << " states and " 
              << params.NUM_VARS << " variables" << std::endl;
}

void HotstateModel::reset() {
    // Reset all states to false
    std::fill(states.begin(), states.end(), false);
    std::fill(stateValue.begin(), stateValue.end(), false);
    std::fill(transitionValue.begin(), transitionValue.end(), false);
    
    // Reset variables to initial values from vardata
    for (size_t i = 0; i < variables.size() && i < vardata.size(); ++i) {
        variables[i] = static_cast<uint8_t>(vardata[i] & 0xFF);
    }
    
    // Reset address and stack
    address = 0;
    returnAddress = 0;
    stackPointer = 0;
    std::fill(std::begin(stack), std::end(stack), 0);
    
    // Reset control signals
    ready = false;
    lhs = false;
    forcedJmp = false;
    jmpadr = false;
    sub = false;
    rtn = false;
    branch = false;
    stateCapture = false;
    switchActive = false;
    fired = false;
    varOrTimer = false;
    
    // Reset microcode fields
    jadr = 0;
    varSel = 0;
    timerSel = 0;
    timerLd = 0;
    switchSel = 0;
    switchAdr = 0;
    
    // Reset timing
    cycleCount = 0;
    
    std::cout << "HotstateModel reset" << std::endl;
}

void HotstateModel::clock() {
    if (hlt) {
        return; // Halted, don't do anything
    }
    
    cycleCount++;
    
    // On rising edge
    if (!clk) {
        clk = true;
        
        if (rst) {
            reset();
            return;
        }
        
        // Execute microcode at current address
        executeMicrocode();
        
        // Update states based on microcode
        updateStates();
        
        // Handle control logic
        handleControlLogic();
        
        // Calculate next address
        handleNextAddress();
    } else {
        clk = false;
    }
}

void HotstateModel::executeMicrocode() {
    if (address >= smdata.size()) {
        throw SimulatorException("Address " + std::to_string(address) + 
                               " exceeds microcode memory size " + std::to_string(smdata.size()));
    }
    
    uint64_t microcode = smdata[address];
    extractMicrocodeFields(microcode);
    
    // Handle switch if active
    if (switchActive) {
        handleSwitch();
    }
    
    // Handle variable selection and comparison
    handleVariables();
}

void HotstateModel::extractMicrocodeFields(uint64_t microcode) {
    // Extract state bits (lower 2*NUM_STATES bits)
    for (uint32_t i = 0; i < params.NUM_STATES; ++i) {
        stateValue[i] = getBit(microcode, i);
        transitionValue[i] = getBit(microcode, params.NUM_STATES + i);
    }
    
    // Extract control bits (remaining bits)
    uint64_t controlBits = microcode >> (2 * params.NUM_STATES);
    
    // Extract control fields based on parameter widths
    uint32_t bitOffset = 0;
    
    jadr = extractBits(controlBits, bitOffset, params.JADR_WIDTH);
    bitOffset += params.JADR_WIDTH;
    
    varSel = extractBits(controlBits, bitOffset, params.VARSEL_WIDTH);
    bitOffset += params.VARSEL_WIDTH;
    
    timerSel = extractBits(controlBits, bitOffset, params.TIMERSEL_WIDTH);
    bitOffset += params.TIMERSEL_WIDTH;
    
    timerLd = extractBits(controlBits, bitOffset, params.TIMERLD_WIDTH);
    bitOffset += params.TIMERLD_WIDTH;
    
    switchSel = extractBits(controlBits, bitOffset, params.SWITCH_SEL_WIDTH);
    bitOffset += params.SWITCH_SEL_WIDTH;
    
    switchAdr = extractBits(controlBits, bitOffset, params.SWITCH_ADR_WIDTH);
    bitOffset += params.SWITCH_ADR_WIDTH;
    
    stateCapture = getBit(controlBits, bitOffset);
    bitOffset += params.STATE_CAPTURE_WIDTH;
    
    varOrTimer = getBit(controlBits, bitOffset);
    bitOffset += params.VAR_OR_TIMER_WIDTH;
    
    branch = getBit(controlBits, bitOffset);
    bitOffset += params.BRANCH_WIDTH;
    
    forcedJmp = getBit(controlBits, bitOffset);
    bitOffset += params.FORCED_JMP_WIDTH;
    
    sub = getBit(controlBits, bitOffset);
    bitOffset += params.SUB_WIDTH;
    
    rtn = getBit(controlBits, bitOffset);
    bitOffset += params.RTN_WIDTH;
}

void HotstateModel::updateStates() {
    if (stateCapture) {
        for (uint32_t i = 0; i < params.NUM_STATES; ++i) {
            if (transitionValue[i]) {
                states[i] = stateValue[i];
            }
        }
    }
}

void HotstateModel::handleControlLogic() {
    // Calculate lhs (left-hand side) based on variable selection
    if (params.NUM_VARS > 0) {
        if (varSel < variables.size()) {
            lhs = (variables[varSel] != 0);
        } else {
            lhs = true; // Default if varSel is out of range
        }
    } else {
        lhs = true; // No variables, always true
    }
    
    // Calculate fired signal based on various conditions
    fired = (lhs && branch) || forcedJmp || rtn || switchActive;
    
    // Calculate jmpadr signal
    jmpadr = fired;
}

void HotstateModel::handleSwitch() {
    if (params.NUM_SWITCHES > 0) {
        // Calculate switch address
        uint32_t switchAddr = calculateSwitchAddress();
        
        if (switchAddr < switchdata.size()) {
            switchAdr = switchdata[switchAddr];
        } else {
            switchAdr = 0; // Default if out of range
        }
    }
}

uint32_t HotstateModel::calculateSwitchAddress() {
    // Combine jadr and switchAdr to form the switch memory address
    // This matches the Verilog: assign memadr = {jadr,switch_offset_adr};
    uint32_t addr = (jadr << params.SWITCH_OFFSET_BITS) | switchAdr;
    return addr;
}

void HotstateModel::handleVariables() {
    // Variable handling is done in handleControlLogic
    // Additional variable operations could be added here
}

void HotstateModel::handleNextAddress() {
    uint32_t nextAddress = address;
    
    if (fired) {
        if (switchActive) {
            nextAddress = switchAdr;
        } else if (rtn && stackPointer > 0) {
            stackPointer--;
            nextAddress = stack[stackPointer];
        } else if (jmpadr) {
            nextAddress = jadr;
        }
    } else {
        nextAddress = address + 1;
    }
    
    // Handle subroutine call
    if (sub && stackPointer < 16) {
        stack[stackPointer] = address + 1;
        stackPointer++;
    }
    
    // Wrap around if we exceed memory size
    if (nextAddress >= params.NUM_WORDS) {
        nextAddress = 0;
    }
    
    address = nextAddress;
    
    // Update ready signal (simplified)
    ready = true;
}

void HotstateModel::setInputs(const std::vector<uint8_t>& inputs) {
    for (size_t i = 0; i < inputs.size() && i < variables.size(); ++i) {
        variables[i] = inputs[i];
    }
}

std::vector<uint8_t> HotstateModel::getOutputs() const {
    // For now, return the current state as output
    std::vector<uint8_t> outputs;
    for (bool state : states) {
        outputs.push_back(state ? 1 : 0);
    }
    return outputs;
}

void HotstateModel::printState() const {
    std::cout << "=== Hotstate Model State ===" << std::endl;
    std::cout << "Cycle: " << cycleCount << std::endl;
    std::cout << "Address: 0x" << std::hex << address << std::dec << std::endl;
    std::cout << "Ready: " << (ready ? "1" : "0") << std::endl;
    std::cout << "States: ";
    for (size_t i = 0; i < states.size(); ++i) {
        std::cout << states[i];
    }
    std::cout << std::endl;
    std::cout << "===========================" << std::endl;
}

void HotstateModel::printMicrocode() const {
    std::cout << "=== Microcode Fields ===" << std::endl;
    std::cout << "jadr: 0x" << std::hex << jadr << std::dec << std::endl;
    std::cout << "varSel: " << varSel << std::endl;
    std::cout << "switchSel: " << switchSel << std::endl;
    std::cout << "switchAdr: " << switchAdr << std::endl;
    std::cout << "stateCapture: " << (stateCapture ? "1" : "0") << std::endl;
    std::cout << "branch: " << (branch ? "1" : "0") << std::endl;
    std::cout << "forcedJmp: " << (forcedJmp ? "1" : "0") << std::endl;
    std::cout << "sub: " << (sub ? "1" : "0") << std::endl;
    std::cout << "rtn: " << (rtn ? "1" : "0") << std::endl;
    std::cout << "========================" << std::endl;
}

void HotstateModel::printControlSignals() const {
    std::cout << "=== Control Signals ===" << std::endl;
    std::cout << "lhs: " << (lhs ? "1" : "0") << std::endl;
    std::cout << "fired: " << (fired ? "1" : "0") << std::endl;
    std::cout << "jmpadr: " << (jmpadr ? "1" : "0") << std::endl;
    std::cout << "switchActive: " << (switchActive ? "1" : "0") << std::endl;
    std::cout << "varOrTimer: " << (varOrTimer ? "1" : "0") << std::endl;
    std::cout << "========================" << std::endl;
}

void HotstateModel::printVariables() const {
    std::cout << "=== Variables ===" << std::endl;
    for (size_t i = 0; i < variables.size(); ++i) {
        std::cout << "var[" << i << "] = 0x" << std::hex << static_cast<uint32_t>(variables[i]) 
                  << std::dec << std::endl;
    }
    std::cout << "=================" << std::endl;
}

void HotstateModel::printStack() const {
    std::cout << "=== Stack ===" << std::endl;
    std::cout << "SP: " << stackPointer << std::endl;
    for (uint32_t i = 0; i < stackPointer; ++i) {
        std::cout << "[" << i << "] = 0x" << std::hex << stack[i] << std::dec << std::endl;
    }
    std::cout << "============" << std::endl;
}

bool HotstateModel::validateState() const {
    if (address >= smdata.size()) {
        return false;
    }
    if (stackPointer > 16) {
        return false;
    }
    return true;
}

std::string HotstateModel::getStateString() const {
    std::string result = "Cycle:" + std::to_string(cycleCount) + 
                        " Addr:0x" + std::to_string(address) +
                        " Ready:" + (ready ? "1" : "0") +
                        " States:";
    for (bool state : states) {
        result += state ? "1" : "0";
    }
    return result;
}

} // namespace HotstateSim
# Hotstate Simulator Code Overview

## Overview

The Hotstate Simulator is a C++ software simulation environment designed to model and execute Hotstate machine behavior. It provides a cycle-accurate simulation of the Hotstate hardware, including state management, variable handling, microcode execution, and stimulus-response modeling. The simulator supports multiple output formats, interactive debugging, and comprehensive analysis capabilities.

## System Architecture

The simulator architecture:

```
┌─────────────────────────────────────────────────────────────┐
│                    Main Application (main.cpp)              │
└─────────────────────┬───────────────────────────────────────┘
                      │
┌─────────────────────┴───────────────────────────────────────┐
│                Simulator (Core Engine)                      │
│  ┌──────────────┬──────────────┬──────────────────────────┐ │
│  │ MemoryLoader │StimulusParser│      OutputLogger        │ │
│  └──────────────┴──────────────┴──────────────────────────┘ │
└─────────────────────┬───────────────────────────────────────┘
                      │
┌─────────────────────┴────────────────────────────────────────┐
│              HotstateModel (Behavioral Model)                │
│┌─────────────┬─────────────┬─────────────┬─────────────────┐ │
││ State Regs  │Control Logic│ Microcode   │ Memory Interface│ │
│└─────────────┴─────────────┴─────────────┴─────────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

## Component Roles & Responsibilities

### 1. Simulator (Core Orchestrator)

**File:** [`sim/src/simulator.cpp`](../sim/src/simulator.cpp:1)

**Role:** Central orchestrator that coordinates all simulator components and manages simulation lifecycle.

**Key Responsibilities:**
- **Simulation Control:** Manages simulation states (IDLE, LOADING, READY, RUNNING, PAUSED, FINISHED, ERROR)
- **Component Integration:** Coordinates between MemoryLoader, HotstateModel, StimulusParser, and OutputLogger
- **Cycle Execution:** Orchestrates the main simulation loop, applying stimulus and advancing the clock
- **Debug Support:** Provides interactive debugging, breakpoints, and watch functionality
- **Configuration Management:** Handles simulator configuration and validation
- **Progress Tracking:** Monitors simulation progress and statistics

**Key Methods:**
- `initialize()` - Sets up all components and validates configuration
- `run()` - Executes the main simulation loop
- `step()` - Executes a specified number of cycles
- `pause()` / `reset()` - Simulation control functions
- `addStateBreakpoint()` / `addAddressBreakpoint()` - Debug features

### 2. HotstateModel (Behavioral Hardware Model)

**File:** [`sim/src/hotstate_model.cpp`](../sim/src/hotstate_model.cpp:1)

**Role:** Cycle-accurate behavioral model of the Hotstate hardware.

**Key Responsibilities:**
- **Microcode Execution:** Fetches and executes microcode instructions from smdata memory
- **State Management:** Maintains and updates state registers based on microcode
- **Control Logic:** Implements Hotstate control signals (lhs, fired, jmpadr, etc.)
- **Variable Handling:** Manages variable selection and comparison operations
- **Switch Management:** Handles switch memory addressing and activation
- **Stack Operations:** Implements subroutine call/return functionality
- **Clock Domain:** Simulates synchronous behavior with clock and reset signals

**Internal Components:**
- **State Registers:** `states[]` - Current state values
- **Variables:** `variables[]` - Variable storage and selection
- **Control Signals:** `lhs`, `fired`, `jmpadr`, `branch`, etc.
- **Microcode Fields:** `jadr`, `varSel`, `switchSel`, etc.
- **Stack:** Simple 16-level stack for subroutine calls

**Key Methods:**
- `clock()` - Executes one clock cycle of behavior
- `executeMicrocode()` - Fetches and decodes microcode
- `updateStates()` - Updates state registers based on microcode
- `handleControlLogic()` - Computes control signals
- `setInputs()` / `getOutputs()` - Input/output interface

### 3. MemoryLoader (Configuration & Data Manager)

**File:** [`sim/src/memory_loader.cpp`](../sim/src/memory_loader.cpp:1)

**Role:** Loads and manages all configuration and memory data from files.

**Key Responsibilities:**
- **Memory File Loading:** Loads vardata, switchdata, and smdata memory files
- **Parameter Parsing:** Parses Verilog parameter files (.vh) and extracts configuration
- **Symbol Table Management:** Supports both TOML and text format symbol tables
- **Data Validation:** Validates loaded data integrity and consistency
- **Memory Interface:** Provides read-only access to loaded data

**File Types Handled:**
- `*_vardata.mem` - Variable initial values and lookup tables
- `*_switchdata.mem` - Switch memory data for state transitions
- `*_smdata.mem` - Microcode program memory (64-bit instructions)
- `*_params.vh` - Verilog parameter definitions
- `*_symbols.toml` / `*_symbols.txt` - Variable and state name mappings

**Key Methods:**
- `loadFromBasePath()` - Loads all memory files from a base path
- `loadVardata()` / `loadSwitchdata()` / `loadSmdata()` - Individual file loaders
- `parseParameterFile()` - Parses Verilog parameter files
- `loadSymbolTableTOML()` / `loadSymbolTableText()` - Symbol table parsers

### 4. StimulusParser (Input Management)

**File:** [`sim/src/stimulus_parser.cpp`](../sim/src/stimulus_parser.cpp:1)

**Role:** Parses and manages input stimulus files that drive the simulation.

**Key Responsibilities:**
- **File Parsing:** Parses stimulus files with cycle-specific input values
- **Input Scheduling:** Manages timing of input application to the simulation
- **Data Validation:** Validates stimulus data for consistency and completeness
- **Input Interpolation:** Provides input values for cycles without explicit entries

**File Format:**
```
cycle,input0,input1,input2,... # comment
0,0xFF,0x00,0x01 # First cycle
1,0xFE,0x01,0x02
```

**Key Methods:**
- `loadStimulus()` - Loads and parses stimulus file
- `getInputs()` - Gets input values for a specific cycle
- `parseLine()` - Parses individual stimulus entries
- `validate()` - Validates loaded stimulus data

### 5. OutputLogger (Results Management)

**File:** [`sim/src/output_logger.cpp`](../sim/src/output_logger.cpp:1)

**Role:** Collects, formats, and outputs simulation results in various formats.

**Key Responsibilities:**
- **Data Collection:** Logs simulation state and output data each cycle
- **Format Conversion:** Supports console, VCD, CSV, and JSON output formats
- **File Management:** Handles output file creation and management
- **Analysis Support:** Provides statistical analysis of simulation results
- **Real-time Output:** Supports real-time console output during simulation

**Supported Formats:**
- **Console:** Human-readable text output to console
- **VCD:** Value Change Dump for waveform viewing
- **CSV:** Comma-separated values for spreadsheet analysis
- **JSON:** Structured data for programmatic processing

**Key Methods:**
- `logCycle()` - Logs one cycle of simulation data
- `writeConsoleEntry()` / `writeVCDEntry()` / etc. - Format-specific writers
- `printStatistics()` - Generates simulation statistics
- `exportToFile()` - Exports logged data to external files

### 6. Utils (Helper Functions)

**File:** [`sim/src/utils.cpp`](../sim/src/utils.cpp:1)

**Role:** Provides common utility functions across all simulator components.

**Key Responsibilities:**
- **String Processing:** Trimming, splitting, and parsing utilities
- **File Operations:** File existence checks and path manipulation
- **Number Parsing:** Hex and decimal number parsing
- **Bit Manipulation:** Bit extraction and manipulation utilities
- **Error Handling:** Custom exception classes for error management

## End-to-End Dataflow

### Initialization Phase

```
1. Command Line Args → Main Application
   ↓
2. Config Object → Simulator Constructor
   ↓
3. Simulator.initialize()
   ├─ MemoryLoader.loadFromBasePath()
   │  ├─ Load vardata.mem → vardata[]
   │  ├─ Load switchdata.mem → switchdata[]
   │  ├─ Load smdata.mem → smdata[]
   │  └─ Load params.vh → Parameters
   ├─ StimulusParser.loadStimulus()
   │  └─ Parse stimulus.txt → StimulusEntry[]
   ├─ OutputLogger initialization
   │  └─ Create logger based on format
   └─ HotstateModel construction
      └─ Initialize with MemoryLoader data
```

### Execution Phase (Per Cycle)

```
1. Simulator main loop iteration
   ├─ Check breakpoints
   └─ applyStimulus(currentCycle)
      └─ StimulusParser.getInputs(cycle) → inputs[]
         └─ HotstateModel.setInputs(inputs)
   ↓
2. HotstateModel.clock() - EXECUTE ONE CYCLE
   ├─ executeMicrocode()
   │  ├─ Fetch smdata[address] → microcode
   │  └─ extractMicrocodeFields() → jadr, varSel, etc.
   ├─ updateStates()
   │  └─ Update states[] based on microcode
   ├─ handleControlLogic()
   │  └─ Calculate lhs, fired, jmpadr
   ├─ handleSwitch()
   │  └─ Calculate switchAdr from switchdata
   └─ handleNextAddress()
      └─ Calculate next address, handle stack
   ↓
3. OutputLogger.logCycle()
   ├─ Collect HotstateModel state
   ├─ Format according to output format
   └─ Write to file/console
   ↓
4. Update cycle counters
   └─ currentCycle++, cyclesSinceStart++
```

### Shutdown Phase

```
1. Simulation completion detection
   ├─ Max cycles reached
   ├─ Breakpoint hit
   └─ Error condition
   ↓
2. Final processing
   ├─ OutputLogger.flush()
   ├─ Print statistics (if verbose)
   └─ Export results (if requested)
   ↓
3. Cleanup
   ├─ Close output files
   └─ Destroy simulator objects
```

## Input & Output Interfaces

### Input Interfaces

#### 1. Command Line Inputs
- **Base Path:** `-b, --base PATH` - Path to memory files
- **Stimulus File:** `-s, --stimulus FILE` - Input stimulus file
- **Output Configuration:** `-o, --output FILE` and `-f, --format FORMAT`
- **Simulation Control:** `-m, --max-cycles NUM`, `-d, --debug`
- **Debug Options:** `--breakpoint-state N`, `--breakpoint-addr ADDR`

#### 2. Memory Files Input
- **`*_vardata.mem`:** Variable initial values and lookup data
- **`*_switchdata.mem`:** Switch transition data
- **`*_smdata.mem`:** Microcode program (64-bit instructions)
- **`*_params.vh`:** Hardware configuration parameters

#### 3. Stimulus File Input
- **Format:** CSV with cycle numbers and input values
- **Timing:** Absolute cycle numbers with optional comments
- **Data:** Hex or decimal input values for each cycle

#### 4. Symbol Table Input (Optional)
- **`*_symbols.toml`:** TOML format variable/state names
- **`*_symbols.txt`:** Text format for backward compatibility

### Output Interfaces

#### 1. Console Output
- **Real-time:** Live cycle-by-cycle output during simulation
- **Statistics:** Final simulation summary and statistics
- **Debug:** Interactive debugger commands and responses

#### 2. File Outputs
- **VCD Files:** Waveform data for external viewers (GTKWave, etc.)
- **CSV Files:** Tabular data for spreadsheet analysis
- **JSON Files:** Structured data for programmatic analysis

#### 3. Data Content
Each output includes:
- **Cycle Number:** Current simulation cycle
- **Program Counter:** Current microcode address
- **State Values:** All state register values
- **Control Signals:** lhs, fired, jmpadr, ready, etc.
- **Variable Values:** Current variable values
- **Input Values:** Applied inputs for current cycle
- **Output Values:** Generated outputs for current cycle

### Interactive Interfaces

#### 1. Debugger Commands
- **Simulation Control:** run, step, continue, pause, reset
- **State Inspection:** state, vars, microcode, memory, stack
- **Breakpoint Management:** bp state, bp addr, bp clear
- **Manual Control:** set input, set var, watch expressions

#### 2. Analysis Features
- **Watch Points:** Monitor variable/state changes
- **Statistics:** Activity analysis, transition counts
- **Memory Inspection:** Direct memory access and display
- **Export:** Convert between output formats

## Configuration Parameters

### Hardware Configuration (from params.vh)
- **STATE_WIDTH:** Number of state bits
- **NUM_STATES:** Number of state registers (2^STATE_WIDTH)
- **NUM_VARS:** Number of variables
- **NUM_WORDS:** Microcode memory size
- **JADR_WIDTH:** Jump address field width
- **VARSEL_WIDTH:** Variable selection field width
- **SWITCH_*:*** Switch-related parameter widths

### Simulator Configuration
- **Max Cycles:** Simulation termination condition
- **Debug Mode:** Enable interactive debugger
- **Output Format:** Console, VCD, CSV, or JSON
- **Real-time Output:** Live console updates
- **Breakpoints:** State and address breakpoints

## Error Handling

The simulator implements comprehensive error handling:

- **File I/O Errors:** Missing or corrupted input files
- **Data Validation:** Parameter validation and consistency checks
- **Runtime Errors:** Invalid memory accesses and instruction execution
- **Configuration Errors:** Invalid command line arguments
- **Simulation Errors:** Breakpoint conditions and halt signals

## Performance Considerations

- **Memory Efficiency:** On-demand memory loading and processing
- **Cycle Performance:** Optimized microcode execution loop
- **Output Efficiency:** Batched output and configurable logging levels
- **Debug Overhead:** Minimal overhead when debug mode disabled

This simulator provides a comprehensive platform for Hotstate hardware simulation, analysis, and validation, supporting both automated testing and interactive development workflows.
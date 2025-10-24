#include "simulator.h"
#include "utils.h"
#include <iostream>
#include <iomanip>
#include <getopt.h>
#include <cctype>

namespace HotstateSim {

void printUsage(const char* programName) {
    std::cout << "Hotstate Machine Simulator" << std::endl;
    std::cout << "Usage: " << programName << " [OPTIONS]" << std::endl;
    std::cout << std::endl;
    std::cout << "Required Options:" << std::endl;
    std::cout << "  -b, --base PATH          Base path for memory files (without extension)" << std::endl;
    std::cout << std::endl;
    std::cout << "Optional Options:" << std::endl;
    std::cout << "  -s, --stimulus FILE      Input stimulus file" << std::endl;
    std::cout << "  -o, --output FILE        Output file (for non-console formats)" << std::endl;
    std::cout << "  -f, --format FORMAT      Output format (console|vcd|csv|json) [default: console]" << std::endl;
    std::cout << "  -m, --max-cycles NUM     Maximum number of cycles to simulate [default: 1000]" << std::endl;
    std::cout << "  -d, --debug              Enable interactive debug mode" << std::endl;
    std::cout << "  -v, --verbose            Enable verbose output" << std::endl;
    std::cout << "  -q, --quiet              Suppress non-error output" << std::endl;
    std::cout << "  --no-realtime            Disable real-time output" << std::endl;
    std::cout << "  --breakpoint-state N     Add state breakpoint" << std::endl;
    std::cout << "  --breakpoint-addr ADDR   Add address breakpoint (hex)" << std::endl;
    std::cout << "  --step NUM               Step mode: run NUM cycles at a time" << std::endl;
    std::cout << "  --export FILE            Export results to FILE" << std::endl;
    std::cout << "  --export-format FORMAT   Export format (csv|json) [default: csv]" << std::endl;
    std::cout << "  -h, --help               Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << programName << " -b test_hybrid_varsel -s stimulus.txt" << std::endl;
    std::cout << "  " << programName << " -b test_hybrid_varsel -f vcd -o trace.vcd" << std::endl;
    std::cout << "  " << programName << " -b test_hybrid_varsel -d" << std::endl;
    std::cout << "  " << programName << " -b test_hybrid_varsel -m 10000 --export results.csv" << std::endl;
}

OutputFormat parseOutputFormat(const std::string& format) {
    if (format == "console") return OutputFormat::CONSOLE;
    if (format == "vcd") return OutputFormat::VCD;
    if (format == "csv") return OutputFormat::CSV;
    if (format == "json") return OutputFormat::JSON;
    
    throw SimulatorException("Invalid output format: " + format);
}

SimulatorConfig parseCommandLine(int argc, char* argv[]) {
    SimulatorConfig config;
    
    static struct option long_options[] = {
        {"base", required_argument, 0, 'b'},
        {"stimulus", required_argument, 0, 's'},
        {"output", required_argument, 0, 'o'},
        {"format", required_argument, 0, 'f'},
        {"max-cycles", required_argument, 0, 'm'},
        {"debug", no_argument, 0, 'd'},
        {"verbose", no_argument, 0, 'v'},
        {"quiet", no_argument, 0, 'q'},
        {"no-realtime", no_argument, 0, 1001},
        {"breakpoint-state", required_argument, 0, 1002},
        {"breakpoint-addr", required_argument, 0, 1003},
        {"step", required_argument, 0, 1004},
        {"export", required_argument, 0, 1005},
        {"export-format", required_argument, 0, 1006},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "b:s:o:f:m:dvqh", long_options, &option_index)) != -1) {
        switch (c) {
            case 'b':
                config.basePath = optarg;
                break;
                
            case 's':
                config.stimulusFile = optarg;
                break;
                
            case 'o':
                config.outputFile = optarg;
                break;
                
            case 'f':
                try {
                    config.outputFormat = parseOutputFormat(optarg);
                } catch (const SimulatorException& e) {
                    throw SimulatorException("Invalid format: " + std::string(optarg));
                }
                break;
                
            case 'm':
                try {
                    config.maxCycles = static_cast<uint32_t>(std::stoul(optarg));
                } catch (const std::exception& e) {
                    throw SimulatorException("Invalid max-cycles value: " + std::string(optarg));
                }
                break;
                
            case 'd':
                config.debugMode = true;
                config.verbose = true;
                break;
                
            case 'v':
                config.verbose = true;
                break;
                
            case 'q':
                config.verbose = false;
                config.realTimeOutput = false;
                break;
                
            case 1001: // --no-realtime
                config.realTimeOutput = false;
                break;
                
            case 1002: // --breakpoint-state
                try {
                    uint32_t state = static_cast<uint32_t>(std::stoul(optarg));
                    config.breakpointStates.push_back(state);
                    config.enableBreakpoints = true;
                } catch (const std::exception& e) {
                    throw SimulatorException("Invalid breakpoint state: " + std::string(optarg));
                }
                break;
                
            case 1003: // --breakpoint-addr
                try {
                    uint32_t addr = parseHex(optarg);
                    config.breakpointAddresses.push_back(addr);
                    config.enableBreakpoints = true;
                } catch (const SimulatorException& e) {
                    throw SimulatorException("Invalid breakpoint address: " + std::string(optarg));
                }
                break;
                
            case 1004: // --step
                try {
                    config.cycleStep = static_cast<uint32_t>(std::stoul(optarg));
                } catch (const std::exception& e) {
                    throw SimulatorException("Invalid step value: " + std::string(optarg));
                }
                break;
                
            case 1005: // --export
                // Store for later use after simulation
                break;
                
            case 1006: // --export-format
                // Store for later use after simulation
                break;
                
            case 'h':
                printUsage(argv[0]);
                exit(0);
                
            case '?':
                throw SimulatorException("Unknown option. Use --help for usage information.");
                
            default:
                throw SimulatorException("Error parsing command line options.");
        }
    }
    
    // Check required options
    if (config.basePath.empty()) {
        throw SimulatorException("Base path is required. Use --help for usage information.");
    }
    
    return config;
}

int runInteractiveMode(Simulator& simulator) {
    std::cout << "Interactive Mode - Type 'help' for commands" << std::endl;
    
    std::string line;
    while (true) {
        std::cout << "sim> ";
        std::getline(std::cin, line);
        
        if (line.empty()) continue;
        
        std::istringstream iss(line);
        std::string command;
        iss >> command;
        
        if (command == "help") {
            std::cout << "=== Debugger Commands ===" << std::endl;
            std::cout << "Simulation Control:" << std::endl;
            std::cout << "  run              - Run simulation until breakpoint or end" << std::endl;
            std::cout << "  step [N]         - Step N cycles (default: 1)" << std::endl;
            std::cout << "  continue         - Continue from breakpoint" << std::endl;
            std::cout << "  pause            - Pause simulation" << std::endl;
            std::cout << "  reset            - Reset simulation" << std::endl;
            std::cout << "  quit             - Exit simulator" << std::endl;
            std::cout << "  exit             - Exit simulator (alias for quit)" << std::endl;
            std::cout << std::endl;
            std::cout << "Inspection Commands:" << std::endl;
            std::cout << "  state            - Show current state" << std::endl;
            std::cout << "  vars             - Show variables/outputs" << std::endl;
            std::cout << "  microcode        - Show current microcode instruction" << std::endl;
            std::cout << "  memory [start] [count] - Inspect memory (default: 0, 16)" << std::endl;
            std::cout << "  stack            - Show call stack" << std::endl;
            std::cout << "  signals          - Show control signals" << std::endl;
            std::cout << "  inputs           - Show current input values" << std::endl;
            std::cout << "  watch            - Evaluate all watch expressions" << std::endl;
            std::cout << std::endl;
            std::cout << "Breakpoint Commands:" << std::endl;
            std::cout << "  bp state N       - Add state breakpoint" << std::endl;
            std::cout << "  bp addr HEX      - Add address breakpoint" << std::endl;
            std::cout << "  bp clear         - Clear all breakpoints" << std::endl;
            std::cout << "  bp list          - List breakpoints" << std::endl;
            std::cout << std::endl;
            std::cout << "Watch Commands:" << std::endl;
            std::cout << "  watch var N      - Add variable watch" << std::endl;
            std::cout << "  watch state N    - Add state watch" << std::endl;
            std::cout << "  watch clear      - Clear all watches" << std::endl;
            std::cout << "  watch list       - List watches" << std::endl;
            std::cout << std::endl;
            std::cout << "Manual Control:" << std::endl;
            std::cout << "  set input N VAL  - Set input N to value VAL" << std::endl;
            std::cout << "  set input NAME VAL - Set input NAME to value VAL (if symbol table available)" << std::endl;
            std::cout << "  set var N VAL    - Set variable N to value VAL" << std::endl;
            std::cout << "  set var NAME VAL - Set variable NAME to value VAL (if symbol table available)" << std::endl;
            std::cout << "  info             - Show current instruction info" << std::endl;
            std::cout << "  stats            - Show simulation statistics" << std::endl;
            
        } else if (command == "run") {
            simulator.debugContinue();
            simulator.run();

        } else if (command == "step") {
            uint32_t steps = 1;
            iss >> steps;
            for (uint32_t i = 0; i < steps; ++i) {
                if (!simulator.debugStep()) break;
            }

        } else if (command == "continue") {
            simulator.debugContinue();
            simulator.run();

        } else if (command == "pause") {
            simulator.debugPause();

        } else if (command == "reset") {
            simulator.reset();

        } else if (command == "state") {
            simulator.inspectState();

        } else if (command == "vars") {
            simulator.inspectVariables();

        } else if (command == "microcode") {
            simulator.inspectMicrocode();

        } else if (command == "memory") {
            uint32_t start = 0, count = 16;
            iss >> start >> count;
            simulator.inspectMemory(start, count);

        } else if (command == "stack") {
            simulator.inspectStack();

        } else if (command == "signals") {
            simulator.inspectControlSignals();

        } else if (command == "inputs") {
            simulator.inspectInputs();

        } else if (command == "watch") {
            std::string subcommand;
            iss >> subcommand;

            if (subcommand == "var") {
                uint32_t varIndex;
                iss >> varIndex;
                simulator.addWatchVariable(varIndex);

            } else if (subcommand == "state") {
                uint32_t stateIndex;
                iss >> stateIndex;
                simulator.addWatchState(stateIndex);

            } else if (subcommand == "clear") {
                simulator.clearWatches();

            } else if (subcommand == "list") {
                simulator.listWatches();

            } else {
                simulator.evaluateWatches();
            }

        } else if (command == "set") {
            std::string type, target;
            uint32_t value;

            iss >> type >> target >> value;

            if (type == "input") {
                // Try to parse as name first, then as index
                if (std::isdigit(target[0])) {
                    // It's a numeric index
                    uint32_t index = std::stoul(target);
                    simulator.setInputValue(index, static_cast<uint8_t>(value));
                } else {
                    // It's a variable name
                    if (!simulator.setInputValueByName(target, static_cast<uint8_t>(value))) {
                        std::cout << "Usage: set input <name> <value> or set input <index> <value>" << std::endl;
                    }
                }
            } else if (type == "var") {
                // Try to parse as name first, then as index
                if (std::isdigit(target[0])) {
                    // It's a numeric index
                    uint32_t index = std::stoul(target);
                    simulator.setVariableValue(index, static_cast<uint8_t>(value));
                } else {
                    // It's a variable name
                    if (!simulator.setVariableValueByName(target, static_cast<uint8_t>(value))) {
                        std::cout << "Usage: set var <name> <value> or set var <index> <value>" << std::endl;
                    }
                }
            } else {
                std::cout << "Usage: set input <name> <value>, set input <index> <value>, set var <name> <value>, or set var <index> <value>" << std::endl;
            }

        } else if (command == "info") {
            simulator.printCurrentInstruction();

        } else if (command == "stats") {
            simulator.printStatistics();
            
        } else if (command == "bp") {
            std::string type;
            iss >> type;
            
            if (type == "state") {
                uint32_t state;
                iss >> state;
                simulator.addStateBreakpoint(state);
                std::cout << "Added state breakpoint: " << state << std::endl;
                
            } else if (type == "addr") {
                std::string addrStr;
                iss >> addrStr;
                try {
                    uint32_t addr = parseHex(addrStr);
                    simulator.addAddressBreakpoint(addr);
                    std::cout << "Added address breakpoint: 0x" << std::hex << addr << std::dec << std::endl;
                } catch (const SimulatorException& e) {
                    std::cout << "Error: " << e.what() << std::endl;
                }
                
            } else if (type == "clear") {
                simulator.clearBreakpoints();
                std::cout << "Cleared all breakpoints" << std::endl;
                
            } else if (type == "list") {
                simulator.listBreakpoints();
                
            } else {
                std::cout << "Unknown breakpoint command. Use 'bp state', 'bp addr', 'bp clear', or 'bp list'" << std::endl;
            }
            
        } else if (command == "quit" || command == "exit") {
            break;
            
        } else {
            std::cout << "Unknown command: " << command << ". Type 'help' for available commands." << std::endl;
        }
    }
    
    return 0;
}

} // namespace HotstateSim

int main(int argc, char* argv[]) {
    try {
        using namespace HotstateSim;
        
        // Parse command line
        SimulatorConfig config;
        try {
            config = parseCommandLine(argc, argv);
        } catch (const SimulatorException& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            printUsage(argv[0]);
            return 1;
        }
        
        // Create and initialize simulator
        Simulator simulator(config);

        if (!simulator.initialize()) {
            std::cerr << "Failed to initialize simulator: " << simulator.getLastError() << std::endl;
            return 1;
        }

        // If debug mode is enabled, enter interactive mode
        if (config.debugMode) {
            return runInteractiveMode(simulator);
        }
        
        // Check for export options
        std::string exportFile;
        OutputFormat exportFormat = OutputFormat::CSV;
        
        // Re-parse command line for export options
        for (int i = 1; i < argc; i++) {
            if (std::string(argv[i]) == "--export" && i + 1 < argc) {
                exportFile = argv[i + 1];
            } else if (std::string(argv[i]) == "--export-format" && i + 1 < argc) {
                try {
                    exportFormat = parseOutputFormat(argv[i + 1]);
                } catch (const SimulatorException& e) {
                    std::cerr << "Warning: Invalid export format, using CSV" << std::endl;
                }
            }
        }
        
        // Run simulation
        bool success = false;
        if (config.cycleStep > 1) {
            // Step mode
            std::cout << "Running in step mode with step size " << config.cycleStep << std::endl;
            
            while (simulator.getState() == SimulatorState::READY || 
                   simulator.getState() == SimulatorState::PAUSED) {
                
                if (simulator.getCurrentCycle() >= config.maxCycles) {
                    break;
                }
                
                simulator.step(config.cycleStep);
                
                if (config.verbose) {
                    std::cout << "Cycle: " << simulator.getCurrentCycle() 
                              << " / " << config.maxCycles << std::endl;
                }
                
                // Check if we hit a breakpoint
                if (simulator.getState() == SimulatorState::PAUSED) {
                    std::cout << "Paused at cycle " << simulator.getCurrentCycle() 
                              << ". Press Enter to continue..." << std::endl;
                    std::cin.get();
                }
            }
            
            success = true;
            
        } else {
            // Normal run
            success = simulator.runToCompletion();
        }
        
        if (!success) {
            std::cerr << "Simulation failed: " << simulator.getLastError() << std::endl;
            return 1;
        }
        
        // Print summary if verbose
        if (config.verbose) {
            simulator.printSummary();
        }
        
        // Export results if requested
        if (!exportFile.empty()) {
            if (simulator.exportResults(exportFile, exportFormat)) {
                std::cout << "Results exported to: " << exportFile << std::endl;
            } else {
                std::cerr << "Failed to export results: " << simulator.getLastError() << std::endl;
                return 1;
            }
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }
}
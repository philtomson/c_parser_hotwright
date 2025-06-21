#ifndef VERILOG_GENERATOR_H
#define VERILOG_GENERATOR_H

#include "cfg_to_microcode.h"
#include <stdio.h>
#include <stdbool.h>

// Verilog module configuration
typedef struct {
    char* module_name;
    char* base_filename;
    
    // Hardware parameters (calculated from microcode)
    int num_states;
    int num_vars;
    int num_varsel;
    int num_varsel_bits;
    int num_timers;
    int num_adr_bits;
    int num_ctl_bits;
    int num_words;
    int stack_depth;
    int num_switches;
    int switch_mem_words;
    int num_switch_bits;
    int switch_offset_bits;
    
    // File names
    char* smdata_filename;
    char* vardata_filename;
    
    // Input/output port names
    char** input_names;
    char** output_names;
    int input_count;
    int output_count;
} VerilogModule;

// File generation options
typedef struct {
    bool generate_module;      // Generate main Verilog module
    bool generate_testbench;   // Generate testbench
    bool generate_user_stim;   // Generate user stimulus file
    bool generate_makefile;    // Generate simulation Makefile
    bool generate_all;         // Generate all files
} VerilogGenOptions;

// --- Core HDL Generation Functions ---

// Main generation function
void generate_verilog_hdl(HotstateMicrocode* mc, const char* source_filename, VerilogGenOptions* options);

// Module generation
VerilogModule* create_verilog_module(HotstateMicrocode* mc, const char* base_name);
void generate_verilog_module_file(VerilogModule* vm, const char* filename);
void generate_verilog_testbench_file(VerilogModule* vm, const char* filename);

// Module components
void write_module_header(VerilogModule* vm, FILE* output);
void write_port_declarations(VerilogModule* vm, FILE* output);
void write_wire_assignments(VerilogModule* vm, FILE* output);
void write_hotstate_instantiation(VerilogModule* vm, FILE* output);

// Testbench components
void write_testbench_header(VerilogModule* vm, FILE* output);
void write_testbench_signals(VerilogModule* vm, FILE* output);
void write_testbench_instantiation(VerilogModule* vm, FILE* output);
void write_testbench_stimulus(VerilogModule* vm, FILE* output);

// Support files
void generate_user_stimulus_file(VerilogModule* vm, const char* filename);
void generate_simulation_makefile(VerilogModule* vm, const char* filename);

// --- Parameter Calculation ---

void calculate_hotstate_parameters(VerilogModule* vm, HotstateMicrocode* mc);
int calculate_address_bits(int num_instructions);
int calculate_varsel_bits(int num_inputs);

// --- Utility Functions ---

char* extract_module_name(const char* source_filename);
char* generate_verilog_filename(const char* base_name, const char* suffix);
void create_file_names(VerilogModule* vm);

// --- Memory Management ---

void free_verilog_module(VerilogModule* vm);

// --- Validation ---

bool validate_verilog_module(VerilogModule* vm);
bool check_port_names(VerilogModule* vm);
bool check_parameter_ranges(VerilogModule* vm);

// Simulation support files
void generate_sim_main_cpp(VerilogModule* vm, const char* filename);
void generate_verilator_sim_h(VerilogModule* vm, const char* filename);

#endif // VERILOG_GENERATOR_H
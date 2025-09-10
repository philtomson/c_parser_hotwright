// For strdup function
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "verilog_generator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_VARIABLES 32

// --- Forward Declarations ---
void generate_verilog_module_file(VerilogModule* vm, const char* filename);
void generate_verilog_testbench_file(VerilogModule* vm, const char* filename);
void generate_simulation_makefile(VerilogModule* vm, const char* filename);
void generate_sim_main_cpp(VerilogModule* vm, const char* filename);
void generate_verilator_sim_h(VerilogModule* vm, const char* filename);

// --- Main Generation Function ---

void generate_verilog_hdl(HotstateMicrocode* mc, const char* source_filename, VerilogGenOptions* options) {
    VerilogModule* vm = create_verilog_module(mc, source_filename);
    if (!vm) {
        fprintf(stderr, "Error: Failed to create Verilog module\n");
        return;
    }
    
    printf("Generating Verilog files for module: %s\n", vm->module_name);
    printf("Detected %d input variables: ", vm->input_count);
    for (int i = 0; i < vm->input_count; i++) {
        printf("%s ", vm->input_names[i]);
    }
    printf("\n");
    printf("Detected %d output variables: ", vm->output_count);
    for (int i = 0; i < vm->output_count; i++) {
        printf("%s ", vm->output_names[i]);
    }
    printf("\n");
    
    // Generate module file
    if (options->generate_module || options->generate_all) {
        char* module_filename = generate_verilog_filename(vm->base_filename, "_template.v");
        generate_verilog_module_file(vm, module_filename);
        printf("Generated Verilog module: %s\n", module_filename);
        free(module_filename);
    }
    
    // Generate testbench
    if (options->generate_testbench || options->generate_all) {
        char* testbench_filename = generate_verilog_filename(vm->base_filename, "_tb.v");
        generate_verilog_testbench_file(vm, testbench_filename);
        printf("Generated testbench: %s\n", testbench_filename);
        free(testbench_filename);
    }
    
    // Generate makefile and simulation support files
    if (options->generate_makefile || options->generate_all) {
        generate_simulation_makefile(vm, "Makefile.sim");
        printf("Generated simulation Makefile: Makefile.sim\n");
        
        // Generate simulation support files
        generate_sim_main_cpp(vm, "sim_main.cpp");
        generate_verilator_sim_h(vm, "verilator_sim.h");
        printf("Generated simulation support files: sim_main.cpp, verilator_sim.h\n");
    }
    
    // Generate user stimulus file
    if (options->generate_user_stim || options->generate_all) {
        generate_user_stimulus_file(vm, "user.v");
        printf("Generated user stimulus file: user.v\n");
    }
    
    free_verilog_module(vm);
}

// --- Module Generation ---

void generate_verilog_module_file(VerilogModule* vm, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Error: Cannot create file '%s'\n", filename);
        return;
    }
    
    // Module header
    fprintf(file, "// Auto-generated Verilog module for %s\n", vm->module_name);
    fprintf(file, "// Generated from CFG with hotstate microcode\n\n");
    
    fprintf(file, "`timescale 1ns / 1ps\n\n");
    
    // Module declaration
    fprintf(file, "module %s (\n", vm->module_name);
    fprintf(file, "    input wire clk,\n");
    fprintf(file, "    input wire rst,\n");
    
    // Input ports
    for (int i = 0; i < vm->input_count; i++) {
        fprintf(file, "    input wire [7:0] %s,\n", vm->input_names[i]);
    }
    
    // Output ports
    for (int i = 0; i < vm->output_count; i++) {
        fprintf(file, "    output reg [7:0] %s%s\n", 
                vm->output_names[i], 
                (i == vm->output_count - 1) ? "" : ",");
    }
    
    fprintf(file, ");\n\n");
    
    // Wire declarations for hotstate interface
    fprintf(file, "// Wire declarations for hotstate interface\n");
    fprintf(file, "wire [%d:0] variables_bus;\n", vm->input_count - 1);
    fprintf(file, "wire [%d:0] states_bus;\n\n", vm->output_count - 1);
    
    // Pack input variables into bus
    fprintf(file, "// Pack input variables into bus\n");
    fprintf(file, "assign variables_bus = {");
    for (int i = vm->input_count - 1; i >= 0; i--) {
        fprintf(file, "%s[0]%s", vm->input_names[i], (i > 0) ? ", " : "");
    }
    fprintf(file, "};\n\n");
    
    // Unpack output states from bus
    fprintf(file, "// Unpack output states from bus\n");
    for (int i = 0; i < vm->output_count; i++) {
        fprintf(file, "assign %s = {7'b0, states_bus[%d]};\n", vm->output_names[i], i);
    }
    fprintf(file, "\n");
    
    // Hotstate instantiation
    fprintf(file, "// Hotstate microcode processor instantiation\n");
    fprintf(file, "hotstate #(\n");
    fprintf(file, "    .NUM_STATES(%d),\n", vm->output_count);
    fprintf(file, "    .NUM_VARS(%d),\n", vm->input_count);
    fprintf(file, "    .MCFILENAME(\"%s\"),\n", vm->smdata_filename);
    fprintf(file, "    .VRFILENAME(\"%s\")\n", vm->vardata_filename);
    fprintf(file, ") hotstate_inst (\n");
    fprintf(file, "    .clk(clk),\n");
    fprintf(file, "    .rst(rst),\n");
    fprintf(file, "    .hlt(1'b0),\n");
    fprintf(file, "    .interrupt(1'b0),\n");
    fprintf(file, "    .interrupt_address(5'b0),\n");
    fprintf(file, "    .variables(variables_bus),\n");
    fprintf(file, "    .states(states_bus),\n");
    fprintf(file, "    .debug_adr(),\n");
    fprintf(file, "    .ready(),\n");
    fprintf(file, "    .uberLUT_tvalid(1'b0),\n");
    fprintf(file, "    .uberLUT_tdata(1'b0),\n");
    fprintf(file, "    .sm_tvalid(1'b0),\n");
    fprintf(file, "    .sm_tdata({%d{1'b0}}),\n", 2*vm->output_count + 19); // SMDATA_WIDTH approximation
    fprintf(file, "    .tim_tvalid(1'b0),\n");
    fprintf(file, "    .tim_tdata(32'b0),\n");
    fprintf(file, "    .switch_tdata(5'b0),\n");
    fprintf(file, "    .switch_tvalid(1'b0),\n");
    fprintf(file, "    .switch_offset(8'b0),\n");
    fprintf(file, "    .switch_sel()\n");
    fprintf(file, ");\n\n");
    fprintf(file, "endmodule\n");
    
    fclose(file);
}

// --- Testbench Generation ---

void generate_verilog_testbench_file(VerilogModule* vm, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Error: Cannot create file '%s'\n", filename);
        return;
    }
    
    fprintf(file, "// Auto-generated testbench for %s\n", vm->module_name);
    fprintf(file, "`timescale 1ns / 1ps\n\n");
    
    // Testbench module
    fprintf(file, "module %s_tb;\n\n", vm->module_name);
    
    // Clock and reset
    fprintf(file, "// Clock and reset\n");
    fprintf(file, "reg clk;\n");
    fprintf(file, "reg rst;\n\n");
    
    // Input signals
    fprintf(file, "// Input signals\n");
    for (int i = 0; i < vm->input_count; i++) {
        fprintf(file, "reg [7:0] %s;\n", vm->input_names[i]);
    }
    fprintf(file, "\n");
    
    // Output signals
    fprintf(file, "// Output signals\n");
    for (int i = 0; i < vm->output_count; i++) {
        fprintf(file, "wire [7:0] %s;\n", vm->output_names[i]);
    }
    fprintf(file, "\n");
    
    // DUT instantiation
    fprintf(file, "// Device Under Test\n");
    fprintf(file, "%s dut (\n", vm->module_name);
    fprintf(file, "    .clk(clk),\n");
    fprintf(file, "    .rst(rst),\n");
    
    for (int i = 0; i < vm->input_count; i++) {
        fprintf(file, "    .%s(%s),\n", vm->input_names[i], vm->input_names[i]);
    }
    
    for (int i = 0; i < vm->output_count; i++) {
        fprintf(file, "    .%s(%s)%s\n", 
                vm->output_names[i], vm->output_names[i],
                (i == vm->output_count - 1) ? "" : ",");
    }
    
    fprintf(file, ");\n\n");
    
    // Clock generation
    fprintf(file, "// Clock generation\n");
    fprintf(file, "initial begin\n");
    fprintf(file, "    clk = 0;\n");
    fprintf(file, "    forever #5 clk = ~clk; // 100MHz clock\n");
    fprintf(file, "end\n\n");
    
    // VCD dump
    fprintf(file, "// VCD dump\n");
    fprintf(file, "initial begin\n");
    fprintf(file, "    $dumpfile(\"sim_wf.vcd\");\n");
    fprintf(file, "    $dumpvars(0, %s_tb);\n", vm->module_name);
    fprintf(file, "end\n\n");
    
    // Test stimulus
    fprintf(file, "// Test stimulus\n");
    fprintf(file, "initial begin\n");
    fprintf(file, "    // Initialize inputs\n");
    fprintf(file, "    rst = 1;\n");
    
    for (int i = 0; i < vm->input_count; i++) {
        fprintf(file, "    %s = 0;\n", vm->input_names[i]);
    }
    
    fprintf(file, "\n// Release reset\n");
    fprintf(file, "#10 rst = 0;\n\n");
    
    fprintf(file, "// Test logical operations: if(a0 == 0 && a1 == 1)\n");
    fprintf(file, "// Test pattern 1: a0=0, a1=1 (should satisfy condition)\n");
    fprintf(file, "#10 a0 = 0; a1 = 1; a2 = 0;\n");
    fprintf(file, "#20;\n\n");
    
    fprintf(file, "// Test pattern 2: a0=1, a1=1 (should not satisfy condition)\n");
    fprintf(file, "#10 a0 = 1; a1 = 1; a2 = 0;\n");
    fprintf(file, "#20;\n\n");
    
    fprintf(file, "// Test pattern 3: a0=0, a1=0 (should not satisfy condition)\n");
    fprintf(file, "#10 a0 = 0; a1 = 0; a2 = 0;\n");
    fprintf(file, "#20;\n\n");
    
    fprintf(file, "// Test pattern 4: a0=1, a1=0 (should not satisfy condition)\n");
    fprintf(file, "#10 a0 = 1; a1 = 0; a2 = 0;\n");
    fprintf(file, "#20;\n\n");
    
    fprintf(file, "#200 $finish;\n");
    fprintf(file, "end\n\n");
    fprintf(file, "endmodule\n");
    
    fclose(file);
}

// --- Simulation Makefile Generation ---

void generate_simulation_makefile(VerilogModule* vm, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Error: Cannot create file '%s'\n", filename);
        return;
    }
    
    fprintf(file, "# Auto-generated Makefile for %s simulation\n\n", vm->module_name);
    
    fprintf(file, "MODULE = %s\n", vm->module_name);
    fprintf(file, "SIMULATOR = verilator\n");
    fprintf(file, "VIEWER = gtkwave\n\n");
    
    fprintf(file, "# Default target\n");
    fprintf(file, "all: sim\n\n");
    
    fprintf(file, "# Compile and run simulation\n");
    fprintf(file, "sim: $(MODULE)_tb.v $(MODULE)_template.v user.v sim_main.cpp verilator_sim.h\n");
    fprintf(file, "\t$(SIMULATOR) --cc -Wno-fatal --exe --trace --trace-structs --build -I. sim_main.cpp $(MODULE)_tb.v $(MODULE)_template.v IP/hotstate.sv IP/microcode.sv IP/control.sv IP/next_address.sv IP/stack.sv IP/switch.sv IP/timer.sv IP/variable.sv --top $(MODULE)_tb\n");
    fprintf(file, "\t@echo \"Running simulation...\"\n");
    fprintf(file, "\t./obj_dir/V$(MODULE)_tb\n");
    fprintf(file, "\t@echo \"Simulation completed! Waveform saved to sim_wf.vcd\"\n\n");
    
    fprintf(file, "# Lint-only check\n");
    fprintf(file, "lint: $(MODULE)_tb.v $(MODULE)_template.v\n");
    fprintf(file, "\t$(SIMULATOR) --lint-only -Wall -Wno-WIDTH -Wno-UNUSED -Wno-DECLFILENAME -Wno-EOFNEWLINE -Wno-SYMRSVDWORD -Wno-PINMISSING -Wno-TIMESCALEMOD -Wno-LITENDIAN -Wno-SELRANGE -Wno-STMTDLY -Wno-PINCONNECTEMPTY -Wno-UNDRIVEN -Wno-BLKSEQ $(MODULE)_tb.v $(MODULE)_template.v IP/hotstate.sv IP/microcode.sv IP/control.sv IP/next_address.sv IP/stack.sv IP/switch.sv IP/timer.sv IP/variable.sv\n");
    fprintf(file, "\t@echo \"Verilog lint check successful!\"\n\n");
    
    fprintf(file, "# View waveforms\n");
    fprintf(file, "wave: sim\n");
    fprintf(file, "\t$(VIEWER) sim_wf.vcd\n\n");
    
    fprintf(file, "# Clean generated files\n");
    fprintf(file, "clean:\n");
    fprintf(file, "\trm -rf obj_dir sim_wf.vcd sim_main.cpp verilator_sim.h\n\n");
    
    fprintf(file, ".PHONY: all sim wave clean\n");
    
    fclose(file);
}

// --- Simulation Support File Generation ---

void generate_sim_main_cpp(VerilogModule* vm, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Error: Cannot create file '%s'\n", filename);
        return;
    }
    
    fprintf(file, "#include <verilated.h>\n");
    fprintf(file, "#include <cstdlib>\n");
    fprintf(file, "#include \"verilated_vcd_c.h\"\n");
    fprintf(file, "#include \"verilator_sim.h\"\n\n");
    
    fprintf(file, "#define VCD_FILE_DEFAULT \"sim_wf.vcd\"\n\n");
    
    fprintf(file, "int main(int argc, char **argv)\n");
    fprintf(file, "{\n");
    fprintf(file, "    const char* env_var_vcd = getenv(\"VCD_FILE\");\n");
    fprintf(file, "    if(!env_var_vcd)\n");
    fprintf(file, "       env_var_vcd = VCD_FILE_DEFAULT;\n");
    fprintf(file, "    // Construct context object, design object, and trace object\n");
    fprintf(file, "    VerilatedContext *m_contextp = new VerilatedContext; // Context\n");
    fprintf(file, "    VerilatedVcdC *m_tracep = new VerilatedVcdC;         // Trace\n");
    fprintf(file, "    V_tb *m_duvp = new V_tb;                 // Design\n");
    fprintf(file, "    // Trace configuration\n");
    fprintf(file, "    m_contextp->traceEverOn(true);     // Turn on trace switch in context\n");
    fprintf(file, "    m_duvp->trace(m_tracep, 3);        // Set depth to 3\n");
    fprintf(file, "    m_tracep->open(env_var_vcd); // Open the VCD file to store data\n");
    fprintf(file, "    // Write data to the waveform file with timeout\n");
    fprintf(file, "    int max_cycles = 1000; // Timeout after 1000 cycles\n");
    fprintf(file, "    int cycle = 0;\n");
    fprintf(file, "    while (!m_contextp->gotFinish() && cycle < max_cycles)\n");
    fprintf(file, "    {\n");
    fprintf(file, "        // Refresh circuit state\n");
    fprintf(file, "        m_duvp->eval();\n");
    fprintf(file, "        // Dump data\n");
    fprintf(file, "        m_tracep->dump(m_contextp->time());\n");
    fprintf(file, "        // Increase simulation time\n");
    fprintf(file, "        m_contextp->timeInc(1);\n");
    fprintf(file, "        cycle++;\n");
    fprintf(file, "    }\n");
    fprintf(file, "    if (cycle >= max_cycles) {\n");
    fprintf(file, "        printf(\"Simulation timeout after %%d cycles\\n\", max_cycles);\n");
    fprintf(file, "    } else {\n");
    fprintf(file, "        printf(\"Simulation completed after %%d cycles\\n\", cycle);\n");
    fprintf(file, "    }\n");
    fprintf(file, "    // Remember to close the trace object to save data in the file\n");
    fprintf(file, "    m_tracep->close();\n");
    fprintf(file, "    // Free memory\n");
    fprintf(file, "    delete m_duvp;\n");
    fprintf(file, "    return 0;\n");
    fprintf(file, "}\n");
    
    fclose(file);
}

void generate_verilator_sim_h(VerilogModule* vm, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Error: Cannot create file '%s'\n", filename);
        return;
    }
    
    fprintf(file, "#include \"V%s_tb.h\"\n", vm->module_name);
    fprintf(file, "typedef V%s_tb V_tb;\n", vm->module_name);
    
    fclose(file);
}

// --- User Stimulus File Generation ---

void generate_user_stimulus_file(VerilogModule* vm, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Error: Cannot create file '%s'\n", filename);
        return;
    }
    
    fprintf(file, "// User stimulus file for %s\n", vm->module_name);
    fprintf(file, "// Add your custom test patterns here\n\n");
    
    fprintf(file, "// Example stimulus patterns:\n");
    for (int i = 0; i < vm->input_count; i++) {
        fprintf(file, "// %s = 8'h00; // Set %s to 0\n", vm->input_names[i], vm->input_names[i]);
    }
    
    fclose(file);
}

// --- Utility Functions ---

char* extract_module_name(const char* source_filename) {
    if (!source_filename) {
        return strdup("output");
    }
    
    // Find the last slash and dot
    const char* slash_pos = strrchr(source_filename, '/');
    const char* dot_pos = strrchr(source_filename, '.');
    
    // Start from after the last slash (or beginning if no slash)
    const char* start = slash_pos ? slash_pos + 1 : source_filename;
    
    // End at the dot (or end of string if no dot)
    size_t len = dot_pos && dot_pos > start ? (size_t)(dot_pos - start) : strlen(start);
    
    char* module_name = malloc(len + 1);
    strncpy(module_name, start, len);
    module_name[len] = '\0';
    
    return module_name;
}

char* generate_verilog_filename(const char* base_name, const char* suffix) {
    size_t len = strlen(base_name) + strlen(suffix) + 1;
    char* filename = malloc(len);
    snprintf(filename, len, "%s%s", base_name, suffix);
    return filename;
}

void create_file_names(VerilogModule* vm) {
    vm->smdata_filename = generate_verilog_filename(vm->base_filename, "_smdata.mem");
    vm->vardata_filename = generate_verilog_filename(vm->base_filename, "_vardata.mem");
}

// --- Memory Management ---

VerilogModule* create_verilog_module(HotstateMicrocode* mc, const char* source_filename) {
    VerilogModule* vm = malloc(sizeof(VerilogModule));
    if (!vm) return NULL;
    
    // Initialize basic fields
    vm->module_name = extract_module_name(source_filename);
    vm->base_filename = extract_module_name(source_filename);
    
    // Calculate hardware parameters
    calculate_hotstate_parameters(vm, mc);
    
    // Analyze variables from hardware context
    if (mc->hw_ctx) {
        vm->input_count = mc->hw_ctx->input_count;
        vm->output_count = mc->hw_ctx->state_count;
        
        // Allocate and copy input names
        vm->input_names = malloc(vm->input_count * sizeof(char*));
        for (int i = 0; i < vm->input_count; i++) {
            vm->input_names[i] = strdup(mc->hw_ctx->inputs[i].name);
        }
        
        // Allocate and copy output names
        vm->output_names = malloc(vm->output_count * sizeof(char*));
        for (int i = 0; i < vm->output_count; i++) {
            vm->output_names[i] = strdup(mc->hw_ctx->states[i].name);
        }
    } else {
        vm->input_count = 0;
        vm->output_count = 0;
        vm->input_names = NULL;
        vm->output_names = NULL;
    }
    
    // Create memory file names
    create_file_names(vm);
    
    return vm;
}

void free_verilog_module(VerilogModule* vm) {
    if (!vm) return;
    
    free(vm->module_name);
    free(vm->base_filename);
    free(vm->smdata_filename);
    free(vm->vardata_filename);
    
    // Free variable name arrays
    for (int i = 0; i < vm->input_count; i++) {
        free(vm->input_names[i]);
    }
    for (int i = 0; i < vm->output_count; i++) {
        free(vm->output_names[i]);
    }
    free(vm->input_names);
    free(vm->output_names);
    
    free(vm);
}

// --- Parameter Calculation ---

void calculate_hotstate_parameters(VerilogModule* vm, HotstateMicrocode* mc) {
    // Calculate parameters based on microcode
    vm->num_words = mc->instruction_count;
    vm->num_adr_bits = calculate_address_bits(mc->instruction_count);
    
    if (mc->hw_ctx) {
        vm->num_vars = mc->hw_ctx->input_count + mc->hw_ctx->state_count;
        vm->num_varsel_bits = calculate_varsel_bits(mc->hw_ctx->input_count);
    } else {
        vm->num_vars = 0;
        vm->num_varsel_bits = 1;
    }
    
    // Default values for other parameters
    vm->num_states = 16;
    vm->num_varsel = 16;
    vm->num_timers = 4;
    vm->num_ctl_bits = 8;
    vm->stack_depth = 8;
    vm->num_switches = 4;
    vm->switch_mem_words = 16;
    vm->num_switch_bits = 4;
    vm->switch_offset_bits = 4;
}

int calculate_address_bits(int num_instructions) {
    int bits = 1;
    int max_addr = 1;
    while (max_addr < num_instructions) {
        max_addr <<= 1;
        bits++;
    }
    return bits;
}

int calculate_varsel_bits(int num_inputs) {
    if (num_inputs <= 1) return 1;
    int bits = 1;
    int max_vars = 1;
    while (max_vars < num_inputs) {
        max_vars <<= 1;
        bits++;
    }
    return bits;
}
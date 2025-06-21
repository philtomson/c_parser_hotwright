# Implementation Plan: State Machine Microcode Generation for c_parser

## Overview
Add state machine microcode generation capability to your c_parser, targeting hardware control programs with boolean state variables (similar to hotstate). This will extend your existing CFG infrastructure to generate hardware-oriented microcode tables.

## Detailed Implementation Plan

### Phase 1: Core Infrastructure (Foundation)

#### 1.1 New Data Structures
Create new header file `microcode.h`:

```c
// State machine microcode structures
typedef struct {
    uint32_t state;          // Output state assignments
    uint32_t mask;           // State mask
    uint32_t jadr;           // Jump address
    uint32_t varSel;         // Variable selection
    uint32_t timerSel;       // Timer selection
    uint32_t timerLd;        // Timer load
    uint32_t switch_sel;     // Switch selection
    uint32_t switch_adr;     // Switch address
    uint32_t state_capture;  // State capture
    uint32_t var_or_timer;   // Variable or timer flag
    uint32_t branch;         // Branch condition
    uint32_t forced_jmp;     // Forced jump
    uint32_t sub;            // Subroutine call
    uint32_t rtn;            // Return
} MicrocodeInstruction;

typedef struct {
    char* name;
    int state_number;
    bool is_output;
} StateVariable;

typedef struct {
    char* name;
    int var_number;
    bool is_input;
} InputVariable;

typedef struct {
    MicrocodeInstruction* instructions;
    int instruction_count;
    int capacity;
    StateVariable* states;
    int state_count;
    InputVariable* inputs;
    int input_count;
} StateMachine;
```

#### 1.2 Hardware Analysis Module
Create `hw_analyzer.c` and `hw_analyzer.h`:

```c
// Analyze AST for hardware constructs
typedef struct {
    bool* state_vars;        // Boolean variables marked as states
    bool* input_vars;        // Boolean variables marked as inputs
    int num_states;
    int num_inputs;
    char** state_names;
    char** input_names;
} HardwareContext;

HardwareContext* analyze_hardware_constructs(Node* ast);
bool is_state_variable(VarDeclNode* var);
bool is_input_variable(VarDeclNode* var);
```

### Phase 2: CFG to State Machine Translation

#### 2.1 State Machine Builder
Create `sm_builder.c` and `sm_builder.h`:

```c
// Convert CFG to state machine
StateMachine* build_state_machine_from_cfg(CFG* cfg, HardwareContext* hw_ctx);

// Key functions:
MicrocodeInstruction* translate_basic_block(BasicBlock* block, HardwareContext* hw_ctx);
uint32_t encode_state_assignments(BasicBlock* block, HardwareContext* hw_ctx);
uint32_t encode_branch_conditions(BasicBlock* block, HardwareContext* hw_ctx);
uint32_t calculate_jump_address(BasicBlock* current, BasicBlock* target);
```

#### 2.2 Hardware Semantics Mapping
```c
// Map C constructs to hardware operations
typedef enum {
    HW_STATE_ASSIGN,    // LED0 = 1
    HW_INPUT_READ,      // if (a0 == 1)
    HW_BRANCH,          // if/else/while conditions
    HW_JUMP,            // goto/continue/break
    HW_NOP              // no operation
} HardwareOperation;

HardwareOperation classify_statement(Node* stmt);
uint32_t encode_boolean_expression(Node* expr, HardwareContext* hw_ctx);
```

### Phase 3: Microcode Generation

#### 3.1 Microcode Formatter
Create `microcode_output.c`:

```c
void print_microcode_table(StateMachine* sm, FILE* output);
void print_microcode_header(StateMachine* sm, FILE* output);
void print_state_assignments(StateMachine* sm, FILE* output);
void print_variable_mappings(StateMachine* sm, FILE* output);

// Generate hotstate-compatible format:
//               s s                
//               w w s     f        
// a             i i t t   o        
// d       v t   t t a i b r        
// d s     a i t c c t m r c        
// r t m j r m i h h e / a e        
// e a a a S S m s a C v n j s r    
// s t s d e e L e d a a c m u t    
// s e k r l l d l r p r h p b n    
// ---------------------------------
// 0 4 7 0 0 x x x x 1 0 0 0 0 0   main(){
```

#### 3.2 Command Line Integration
Update `main.c`:

```c
// Add new command line flag
static struct option long_options[] = {
    {"dot", no_argument, 0, 'd'},
    {"debug", no_argument, 0, 'g'},
    {"microcode", no_argument, 0, 'm'},  // NEW
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
};

// Usage:
// ./c_parser --microcode led_controller.c
```

### Phase 4: Hardware-Specific Enhancements

#### 4.1 Boolean Variable Detection
Enhance parser to recognize hardware patterns:

```c
// In parser.c - detect state variable declarations
bool LED0 = 0; /* state0 */  // Comment indicates state number
bool LED1 = 0; /* state1 */
bool LED2 = 1; /* state2 */

// Input variables
bool a0, a1, a2;  // Automatically detected as inputs
```

#### 4.2 Hardware Constraint Validation
```c
// Validate hardware constraints
bool validate_hardware_program(Node* ast, HardwareContext* hw_ctx);
bool check_state_assignments(Node* stmt);
bool check_input_usage(Node* expr);
bool validate_control_flow(CFG* cfg);
```

### Phase 5: Integration and Testing

#### 5.1 Integration Points
```c
// In main.c processing flow:
if (microcode_flag) {
    // 1. Parse C code (existing)
    Node* ast = parse_file(filename);
    
    // 2. Build CFG (existing)
    CFG* cfg = build_cfg_from_ast(ast);
    
    // 3. Analyze hardware constructs (NEW)
    HardwareContext* hw_ctx = analyze_hardware_constructs(ast);
    
    // 4. Build state machine (NEW)
    StateMachine* sm = build_state_machine_from_cfg(cfg, hw_ctx);
    
    // 5. Generate microcode (NEW)
    print_microcode_table(sm, stdout);
    
    // 6. Cleanup
    free_state_machine(sm);
    free_hardware_context(hw_ctx);
}
```

#### 5.2 Test Cases
Create test files similar to hotstate examples:

```c
// test_led_simple.c
bool LED0 = 0; /* state0 */
bool LED1 = 0; /* state1 */
bool a0, a1;   // inputs

int main() {
    while(1) {
        if(a0 == 1 && a1 == 0)
            LED0 = 1;
        else
            LED0 = 0;
            
        if(a1 == 1)
            LED1 = 1;
    }
}
```

### Phase 6: Advanced Features

#### 6.1 Optimization
```c
// State machine optimizations
StateMachine* optimize_state_machine(StateMachine* sm);
void eliminate_redundant_states(StateMachine* sm);
void merge_equivalent_states(StateMachine* sm);
```

#### 6.2 Hardware Export
```c
// Generate additional hardware formats
void export_verilog_testbench(StateMachine* sm, const char* filename);
void export_vhdl_entity(StateMachine* sm, const char* filename);
void export_dot_state_diagram(StateMachine* sm, const char* filename);
```

## Implementation Timeline

### Week 1-2: Foundation
- Implement microcode data structures
- Create hardware analysis module
- Add command line flag support

### Week 3-4: Core Translation
- Implement CFG to state machine conversion
- Create microcode instruction encoding
- Basic microcode table output

### Week 5-6: Hardware Semantics
- Implement boolean expression encoding
- Add state assignment detection
- Hardware constraint validation

### Week 7-8: Integration & Testing
- Full integration with existing c_parser
- Comprehensive test suite
- Documentation and examples

## Expected Output Format

```bash
$ ./c_parser --microcode test_led_simple.c

State Machine Microcode derived from test_led_simple.c 

              s s                
              w w s     f        
a             i i t t   o        
d       v t   t t a i b r        
d s     a i t c c t m r c        
r t m j r m i h h e / a e        
e a a a S S m s a C v n j s r    
s t s d e e L e d a a c m u t    
s e k r l l d l r p r h p b n    
---------------------------------
0 1 0 0 0 x x x x 1 0 0 0 0 0   main(){
1 0 0 3 1 x x x x 0 0 1 0 0 0   while (1) {
2 1 0 0 0 x x x x 1 0 0 0 0 0   LED0=1;}
3 0 0 5 2 x x x x 0 0 1 0 0 0   if (a1 == 1) {
4 2 0 0 0 x x x x 1 0 0 0 0 0   LED1=1;}
5 0 0 1 0 x x x x 0 0 0 1 0 0   }

State assignments
state 0 is LED0
state 1 is LED1

Variable inputs
var 0 is a0
var 1 is a1
```

## Benefits

1. **Hardware-Oriented Analysis**: Direct translation from C to state machine microcode
2. **FPGA Integration**: Compatible with existing hotstate toolchain
3. **Educational Value**: Visualize how software maps to hardware
4. **Reusable Infrastructure**: Leverages existing CFG and AST components
5. **Extensible Design**: Easy to add new hardware targets and optimizations

This implementation would make your c_parser a powerful tool for both software analysis and hardware synthesis, bridging the gap between high-level C code and low-level hardware implementation.

# Enhanced Implementation Plan: State Machine Microcode + Verilog HDL Generation for c_parser

## Overview
Extend the previous plan to include **Verilog HDL generation** capability, making your c_parser a complete C-to-hardware synthesis tool like hotstate. This will generate both microcode tables and synthesizable Verilog modules.

## Enhanced Implementation Plan

### Phase 1: Core Infrastructure (Foundation) - SAME AS BEFORE
- Microcode data structures
- Hardware analysis module
- Command line integration

### Phase 2: CFG to State Machine Translation - SAME AS BEFORE
- State machine builder
- Hardware semantics mapping

### Phase 3: Enhanced Microcode + Verilog Generation

#### 3.1 Verilog HDL Generator
Create `verilog_generator.c` and `verilog_generator.h`:

```c
typedef struct {
    char* module_name;
    int num_states;
    int num_inputs;
    int num_outputs;
    char** input_names;
    char** output_names;
    char** state_names;
    int microcode_depth;
    char* memory_file;
} VerilogModule;

// Main Verilog generation functions
void generate_verilog_module(StateMachine* sm, const char* module_name, FILE* output);
void generate_verilog_testbench(StateMachine* sm, const char* module_name, FILE* output);
void generate_memory_files(StateMachine* sm, const char* base_name);

// Verilog component generators
void write_module_header(VerilogModule* vm, FILE* output);
void write_port_declarations(VerilogModule* vm, FILE* output);
void write_wire_assignments(VerilogModule* vm, FILE* output);
void write_hotstate_instantiation(VerilogModule* vm, FILE* output);
void write_testbench_stimulus(VerilogModule* vm, FILE* output);
```

#### 3.2 Memory File Generation
```c
// Generate memory initialization files
void generate_smdata_mem(StateMachine* sm, const char* filename);
void generate_vardata_mem(StateMachine* sm, const char* filename);

// Memory file formats (compatible with hotstate)
// simple_smdata.mem - microcode instructions
// simple_vardata.mem - variable mappings
```

#### 3.3 Enhanced Command Line Interface
Update `main.c`:

```c
// Enhanced command line options
static struct option long_options[] = {
    {"dot", no_argument, 0, 'd'},
    {"debug", no_argument, 0, 'g'},
    {"microcode", no_argument, 0, 'm'},
    {"verilog", no_argument, 0, 'v'},        // NEW: Generate Verilog module
    {"testbench", no_argument, 0, 't'},      // NEW: Generate testbench
    {"all-hdl", no_argument, 0, 'a'},        // NEW: Generate all HDL files
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
};

// Usage examples:
// ./c_parser --verilog led_controller.c          # Generate module only
// ./c_parser --testbench led_controller.c        # Generate testbench only
// ./c_parser --all-hdl led_controller.c          # Generate everything
// ./c_parser --microcode --verilog led_controller.c  # Both formats
```

### Phase 4: Verilog Template System

#### 4.1 Module Template Generation
```c
// Generate hotstate-compatible Verilog module
void generate_module_template(StateMachine* sm, const char* module_name) {
    // Output format similar to hotstate:
    /*
    module simple_sm(
        input a0,
        input a1, 
        input a2,
        input clk,
        input hlt,
        input rst,
        input interrupt,
        input [3:0] interrupt_address,
        output LED0,
        output LED1,
        output LED2,
        output [3:0] debug_adr,
        output ready);

    wire [2:0] states;
    wire [2:0] variables;

    assign LED0 = states[0];
    assign LED1 = states[1];
    assign LED2 = states[2];

    assign variables = { a2, a1, a0 };

    hotstate #(
        .NUM_STATES (3),
        .NUM_VARS (3),
        .NUM_VARSEL (6),
        .NUM_VARSEL_BITS (3),
        .NUM_TIMERS (0),
        .NUM_ADR_BITS (4),
        .NUM_CTL_BITS (17),
        .NUM_WORDS (15),
        .STACK_DEPTH (0),
        .NUM_SWITCHES (0),
        .SWITCH_MEM_WORDS (0),
        .NUM_SWITCH_BITS (1),
        .SWITCH_OFFSET_BITS (0),
        .MCFILENAME ("simple_smdata.mem"),
        .VARFILENAME ("simple_vardata.mem")
    ) hotstate_inst (
        .clk(clk),
        .rst(rst),
        .hlt(hlt),
        .interrupt(interrupt),
        .interrupt_address(interrupt_address),
        .variables(variables),
        .states(states),
        .debug_adr(debug_adr),
        .ready(ready)
    );

    endmodule
    */
}
```

#### 4.2 Testbench Template Generation
```c
void generate_testbench_template(StateMachine* sm, const char* module_name) {
    // Generate comprehensive testbench:
    /*
    `timescale 1ns/1ns

    module simple_tb;

    reg rst, hlt, clk;
    reg interrupt;
    wire [3:0] debug_adr;
    reg [3:0] interrupt_address = 4'h0;

    wire [2:0] variables;
    wire [2:0] states;

    // Output states
    assign LED0 = states[0];
    assign LED1 = states[1];
    assign LED2 = states[2];

    // Input variables
    reg a2, a1, a0;
    assign variables = { a2, a1, a0 };

    wire ready;

    // Instantiate the module under test
    simple_sm uut (
        .a0(a0), .a1(a1), .a2(a2),
        .clk(clk), .hlt(hlt), .rst(rst),
        .interrupt(interrupt),
        .interrupt_address(interrupt_address),
        .LED0(LED0), .LED1(LED1), .LED2(LED2),
        .debug_adr(debug_adr),
        .ready(ready)
    );

    // Clock generation
    always #5 clk = ~clk;

    // Test stimulus
    initial begin
        // Include user.v for custom test patterns
        `include "user.v"
    end

    endmodule
    */
}
```

#### 4.3 User Stimulus File Generation
```c
void generate_user_stimulus(StateMachine* sm, const char* filename) {
    // Generate user.v file for custom test patterns:
    /*
    // User-defined test stimulus
    // Edit this file to create custom test patterns
    
    clk = 0;
    rst = 1;
    hlt = 0;
    interrupt = 0;
    a0 = 0; a1 = 0; a2 = 0;
    
    #10 rst = 0;  // Release reset
    
    // Test pattern 1: a0=0, a1=1 should set LED0=1
    #10 a0 = 0; a1 = 1; a2 = 0;
    #20;
    
    // Test pattern 2: a1=1 should set LED1=1  
    #10 a0 = 0; a1 = 1; a2 = 0;
    #20;
    
    // Add more test patterns here...
    
    #100 $finish;
    */
}
```

### Phase 5: Hardware Parameter Calculation

#### 5.1 Automatic Parameter Generation
```c
typedef struct {
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
} HotstateParameters;

HotstateParameters* calculate_hotstate_parameters(StateMachine* sm);

// Automatic calculation based on state machine analysis:
// - num_states: Count of boolean output variables
// - num_vars: Count of boolean input variables  
// - num_words: Number of microcode instructions
// - num_adr_bits: ceil(log2(num_words))
// - etc.
```

### Phase 6: File Management System

#### 6.1 Multi-File Output
```c
typedef struct {
    char* base_name;
    char* module_file;      // simple_template.v
    char* testbench_file;   // simple_tb.v
    char* smdata_file;      // simple_smdata.mem
    char* vardata_file;     // simple_vardata.mem
    char* user_file;        // user.v
    char* makefile;         // Makefile for simulation
} HDLFileSet;

HDLFileSet* create_hdl_fileset(const char* base_name);
void generate_all_hdl_files(StateMachine* sm, HDLFileSet* files);
void generate_simulation_makefile(HDLFileSet* files);
```

#### 6.2 Makefile Generation
```c
void generate_makefile(const char* module_name) {
    // Generate Makefile for simulation:
    /*
    # Auto-generated Makefile for simple module
    
    MODULE = simple
    
    sim: $(MODULE)_tb.v $(MODULE)_template.v user.v
    	iverilog -o $(MODULE)_sim $(MODULE)_tb.v
    	vvp $(MODULE)_sim
    	
    wave: sim
    	gtkwave $(MODULE).vcd
    	
    clean:
    	rm -f $(MODULE)_sim $(MODULE).vcd
    	
    .PHONY: sim wave clean
    */
}
```

### Phase 7: Integration and Enhanced Testing

#### 7.1 Complete Integration Flow
```c
// Enhanced main.c processing:
if (verilog_flag || testbench_flag || all_hdl_flag) {
    // 1. Parse C code (existing)
    Node* ast = parse_file(filename);
    
    // 2. Build CFG (existing)  
    CFG* cfg = build_cfg_from_ast(ast);
    
    // 3. Analyze hardware constructs
    HardwareContext* hw_ctx = analyze_hardware_constructs(ast);
    
    // 4. Build state machine
    StateMachine* sm = build_state_machine_from_cfg(cfg, hw_ctx);
    
    // 5. Generate HDL files
    char* base_name = extract_base_name(filename);
    HDLFileSet* files = create_hdl_fileset(base_name);
    
    if (verilog_flag || all_hdl_flag) {
        generate_verilog_module(sm, base_name, files->module_file);
    }
    
    if (testbench_flag || all_hdl_flag) {
        generate_verilog_testbench(sm, base_name, files->testbench_file);
        generate_user_stimulus(sm, files->user_file);
    }
    
    if (all_hdl_flag) {
        generate_memory_files(sm, base_name);
        generate_simulation_makefile(files);
    }
    
    // 6. Optional: Also generate microcode table
    if (microcode_flag) {
        print_microcode_table(sm, stdout);
    }
    
    printf("Generated HDL files for module: %s\n", base_name);
    printf("To simulate: make sim\n");
    printf("To view waveforms: make wave\n");
}
```

### Phase 8: Advanced HDL Features

#### 8.1 Simulation Support
```c
// Generate simulation-ready files
void add_simulation_directives(FILE* testbench) {
    fprintf(testbench, 
        "initial begin\n"
        "    $dumpfile(\"%s.vcd\");\n"
        "    $dumpvars(0, %s_tb);\n"
        "end\n", 
        module_name, module_name);
}
```

#### 8.2 Synthesis Attributes
```c
// Add synthesis attributes for FPGA tools
void add_synthesis_attributes(FILE* module) {
    fprintf(module,
        "(* keep_hierarchy = \"yes\" *)\n"
        "(* dont_touch = \"true\" *)\n");
}
```

## Expected Complete Output

```bash
$ ./c_parser --all-hdl led_controller.c

Parsing file: led_controller.c
Analyzing hardware constructs...
Building state machine...
Generating HDL files...

Generated files:
- led_controller_template.v    (Verilog module)
- led_controller_tb.v          (Testbench)
- led_controller_smdata.mem    (Microcode memory)
- led_controller_vardata.mem   (Variable memory)
- user.v                       (User stimulus)
- Makefile                     (Simulation makefile)

To simulate: make sim
To view waveforms: make wave

State Machine Summary:
- 3 output states (LED0, LED1, LED2)
- 3 input variables (a0, a1, a2)
- 15 microcode instructions
- Compatible with hotstate IP core
```

## Benefits of Enhanced Implementation

1. **Complete C-to-Hardware Flow**: From C source to synthesizable Verilog
2. **Hotstate Compatibility**: Generated files work with existing hotstate IP
3. **Simulation Ready**: Includes testbenches and simulation makefiles
4. **Educational Value**: Shows complete hardware synthesis process
5. **Professional Output**: Production-quality HDL generation
6. **Extensible Architecture**: Easy to add new HDL targets (VHDL, SystemVerilog)

This enhanced implementation makes your c_parser a complete hardware synthesis tool, capable of generating both microcode analysis and synthesizable HDL, matching and potentially exceeding hotstate's capabilities while leveraging your existing CFG infrastructure.


#ifndef CFG_TO_MICROCODE_H
#define CFG_TO_MICROCODE_H

#include "cfg.h"
#include "hw_analyzer.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// Hotstate microcode instruction format
typedef struct {
    uint32_t instruction_word;  // Packed 24-bit hotstate instruction
    char* label;               // Human-readable label for debugging
    int address;               // Instruction address in microcode memory
    BasicBlock* source_block;  // Reference to source CFG block
} HotstateInstruction;

// Complete microcode program
typedef struct {
    HotstateInstruction* instructions;
    int instruction_count;
    int instruction_capacity;
    
    HardwareContext* hw_ctx;   // Hardware variable mappings
    CFG* source_cfg;           // Source control flow graph
    char* function_name;       // Function name
    
    // Address mapping for jump resolution
    int* block_addresses;      // block_id -> instruction_address mapping
    int block_count;
    
    // Generation statistics
    int state_assignments;     // Number of state assignment instructions
    int branches;             // Number of branch instructions
    int jumps;                // Number of jump instructions
} HotstateMicrocode;

// Hotstate instruction field definitions (24-bit format)
#define HOTSTATE_STATE_MASK     0x00F000  // Bits 12-15: State assignments
#define HOTSTATE_MASK_MASK      0x000F00  // Bits 8-11:  State mask
#define HOTSTATE_JADR_MASK      0x0000F0  // Bits 4-7:   Jump address
#define HOTSTATE_VARSEL_MASK    0x00000F  // Bits 0-3:   Variable selection

#define HOTSTATE_STATE_SHIFT    12
#define HOTSTATE_MASK_SHIFT     8
#define HOTSTATE_JADR_SHIFT     4
#define HOTSTATE_VARSEL_SHIFT   0

// Control flags (additional bits)
#define HOTSTATE_BRANCH_FLAG    0x010000  // Bit 16: Branch enable
#define HOTSTATE_FORCED_JMP     0x020000  // Bit 17: Forced jump
#define HOTSTATE_STATE_CAP      0x040000  // Bit 18: State capture
#define HOTSTATE_VAR_TIMER      0x080000  // Bit 19: Variable/timer select

// Switch fields (hotstate positions 8 and 9)
#define HOTSTATE_SWITCH_SEL_MASK  0x00F00000  // Bits 20-23: Switch selection (switchsel)
#define HOTSTATE_SWITCH_ADR_MASK  0x01000000  // Bit 24: Switch address flag (switchadr)

#define HOTSTATE_SWITCH_SEL_SHIFT 20
#define HOTSTATE_SWITCH_ADR_SHIFT 24

// Switch memory configuration
#define MAX_SWITCH_ENTRIES      4096    // Total switch memory entries
#define SWITCH_OFFSET_BITS      8       // 256 entries per switch (2^8)
#define MAX_SWITCHES           16       // Maximum number of switches

// --- Core Translation Functions ---

// Main translation function
HotstateMicrocode* cfg_to_hotstate_microcode(CFG* cfg, HardwareContext* hw_ctx);

// Block-level translation
void translate_basic_block(HotstateMicrocode* mc, BasicBlock* block);
void translate_phi_nodes(HotstateMicrocode* mc, BasicBlock* block);
void translate_instructions(HotstateMicrocode* mc, BasicBlock* block);
void translate_control_flow(HotstateMicrocode* mc, BasicBlock* block);

// Instruction encoding
uint32_t encode_state_assignment(SSAInstruction* instr, HardwareContext* hw_ctx);
uint32_t encode_conditional_branch(SSAInstruction* instr, HardwareContext* hw_ctx, int true_addr, int false_addr);
uint32_t encode_unconditional_jump(int target_addr);
uint32_t encode_nop_instruction();

// SSA instruction translation
bool is_state_assignment(SSAInstruction* instr, HardwareContext* hw_ctx);
bool is_input_reference(SSAValue* value, HardwareContext* hw_ctx);
int get_state_bit_from_ssa_value(SSAValue* value, HardwareContext* hw_ctx);
int get_input_number_from_ssa_value(SSAValue* value, HardwareContext* hw_ctx);

// Address resolution
void build_address_mapping(HotstateMicrocode* mc);
void resolve_jump_addresses(HotstateMicrocode* mc);
int get_block_address(HotstateMicrocode* mc, BasicBlock* block);

// Instruction management
void add_hotstate_instruction(HotstateMicrocode* mc, uint32_t instruction_word, const char* label, BasicBlock* source_block);
void resize_instruction_array(HotstateMicrocode* mc);

// --- Output Generation ---

// Hotstate-compatible output
void print_hotstate_microcode_table(HotstateMicrocode* mc, FILE* output);
void print_microcode_header(FILE* output);
void generate_smdata_mem_file(HotstateMicrocode* mc, const char* filename);
void generate_vardata_mem_file(HotstateMicrocode* mc, const char* filename);

// Debug output
void print_microcode_analysis(HotstateMicrocode* mc, FILE* output);
void print_instruction_details(HotstateInstruction* instr, FILE* output);
void print_address_mapping(HotstateMicrocode* mc, FILE* output);

// --- Memory Management ---

HotstateMicrocode* create_hotstate_microcode(CFG* cfg, HardwareContext* hw_ctx);
void free_hotstate_microcode(HotstateMicrocode* mc);

// --- Output Generation Functions (from microcode_output.c) ---

void print_state_assignments(HotstateMicrocode* mc, FILE* output);
void print_variable_mappings(HotstateMicrocode* mc, FILE* output);
char* generate_base_filename(const char* source_filename);
void generate_all_output_files(HotstateMicrocode* mc, const char* source_filename);

// --- Validation ---

bool validate_microcode(HotstateMicrocode* mc);
bool check_address_bounds(HotstateMicrocode* mc);
bool check_variable_references(HotstateMicrocode* mc);

#endif // CFG_TO_MICROCODE_H
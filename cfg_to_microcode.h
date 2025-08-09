#ifndef CFG_TO_MICROCODE_H
#define CFG_TO_MICROCODE_H

#include "cfg.h"
#include "hw_analyzer.h"
#include "ast_to_microcode.h"
#include "microcode_defs.h" // Include new microcode definitions
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// Complete microcode program
typedef struct {
    Code* instructions;
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

    // Dynamic bit-width tracking (from Phase 2)
    int max_state_val;
    int max_mask_val;
    int max_jadr_val;
    int max_varsel_val;
    int max_timersel_val;
    int max_timerld_val;
    int max_switch_sel_val;
    int max_switch_adr_val;
    int max_state_capture_val;
    int max_var_or_timer_val;
    int max_branch_val;
    int max_forced_jmp_val;
    int max_sub_val;
    int max_rtn_val;
} HotstateMicrocode;

// Hotstate instruction field definitions (these will become obsolete for direct MCode access)
// Keeping them for now for compatibility during transition, but they should eventually be removed.
#define HOTSTATE_STATE_MASK     0x00F000
#define HOTSTATE_MASK_MASK      0x000F00
#define HOTSTATE_JADR_MASK      0x0000F0
#define HOTSTATE_JADR_EXT_MASK  0x00F000
#define HOTSTATE_VARSEL_MASK    0x00000F

#define HOTSTATE_STATE_SHIFT    12
#define HOTSTATE_MASK_SHIFT     8
#define HOTSTATE_JADR_SHIFT     4
#define HOTSTATE_JADR_EXT_SHIFT 12
#define HOTSTATE_VARSEL_SHIFT   0

#define HOTSTATE_BRANCH_FLAG    0x010000
#define HOTSTATE_FORCED_JMP     0x020000
#define HOTSTATE_STATE_CAP      0x040000
#define HOTSTATE_VAR_TIMER      0x080000

#define HOTSTATE_SWITCH_SEL_MASK  0x00F00000
#define HOTSTATE_SWITCH_ADR_MASK  0x01000000

#define HOTSTATE_SWITCH_SEL_SHIFT 20
#define HOTSTATE_SWITCH_ADR_SHIFT 24

// Switch memory configuration
#define MAX_SWITCH_ENTRIES      4096    // Total switch memory entries
#define SWITCH_OFFSET_BITS      8       // 256 entries per switch (2^8)
#define MAX_SWITCHES           4       // Maximum number of switches

// --- Core Translation Functions ---

// Main translation function
HotstateMicrocode* cfg_to_hotstate_microcode(CFG* cfg, HardwareContext* hw_ctx);

// Block-level translation
void translate_basic_block(HotstateMicrocode* mc, BasicBlock* block);
void translate_phi_nodes(HotstateMicrocode* mc, BasicBlock* block);
void translate_instructions(HotstateMicrocode* mc, BasicBlock* block);
void translate_control_flow(HotstateMicrocode* mc, BasicBlock* block);

// Instruction encoding
MCode encode_state_assignment(SSAInstruction* instr, HardwareContext* hw_ctx);
MCode encode_conditional_branch(SSAInstruction* instr, HardwareContext* hw_ctx, int true_addr, int false_addr);
MCode encode_unconditional_jump(int target_addr);
MCode encode_nop_instruction();

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
void add_hotstate_instruction(HotstateMicrocode* mc, MCode mcode_val, const char* label, BasicBlock* source_block);
void resize_instruction_array(HotstateMicrocode* mc);

// --- Output Generation ---

// Hotstate-compatible output
// void print_hotstate_microcode_table(CompactMicrocode* mc, FILE* output); // Moved to ast_to_microcode.h
void generate_smdata_mem_file(CompactMicrocode* mc, const char* filename);
void generate_vardata_mem_file(CompactMicrocode* mc, const char* filename);

// Debug output
void print_microcode_analysis(HotstateMicrocode* mc, FILE* output);
// print_instruction_details will need to be updated to take MCode or Code directly
// For now, removing the HotstateInstruction* version to avoid conflicts.
// void print_instruction_details(Code* instr, FILE* output);
void print_address_mapping(HotstateMicrocode* mc, FILE* output);

// --- Memory Management ---

HotstateMicrocode* create_hotstate_microcode(CFG* cfg, HardwareContext* hw_ctx);
void free_hotstate_microcode(HotstateMicrocode* mc);

// --- Output Generation Functions (from microcode_output.c) ---

char* generate_base_filename(const char* source_filename);
void generate_all_output_files(HotstateMicrocode* mc, const char* source_filename);

// --- Validation ---

bool validate_microcode(HotstateMicrocode* mc);
bool check_address_bounds(HotstateMicrocode* mc);
bool check_variable_references(HotstateMicrocode* mc);

#endif // CFG_TO_MICROCODE_H
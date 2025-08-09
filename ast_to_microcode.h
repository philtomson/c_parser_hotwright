#ifndef AST_TO_MICROCODE_H
#define AST_TO_MICROCODE_H

#include "ast.h"
#include "microcode_defs.h"
#include "hw_analyzer.h"
#include <stdio.h>
#include <stdint.h>

typedef enum {
    CONTEXT_TYPE_LOOP,
    CONTEXT_TYPE_LOOP_OR_SWITCH
} ContextSearchType;

typedef struct {
    NodeType loop_type;  // e.g., NODE_WHILE, NODE_FOR, NODE_SWITCH
    int continue_target; // Microcode address for 'continue' statements
    int break_target;    // Microcode address for 'break' statements
} LoopSwitchContext;

// Proposed PendingJump structure
typedef enum {
    JUMP_TYPE_BREAK,
    JUMP_TYPE_CONTINUE,
    JUMP_TYPE_EXIT,
    JUMP_TYPE_DIRECT // For direct address jumps not tied to a context (e.g., if/else branches)
} JumpType;

typedef struct {
    int instruction_index;              // Index in mc->instructions where the MCode is
    int target_instruction_address;     // The symbolic target address (e.g., loop start, exit)
    bool is_exit_jump;                  // True if this jump should target the global :exit
    JumpType jump_type;                 // Type of jump (break, continue, exit)
    int direct_address;                 // Used for JUMP_TYPE_DIRECT, stores the absolute address
} PendingJump;

// Compact microcode structure
typedef struct {
    Code* instructions; // Array of new Code structs
    int instruction_count;
    int instruction_capacity;  // what is this for?
    char* function_name;
    HardwareContext* hw_ctx;
    //Loop/switch context handling:
    LoopSwitchContext* loop_switch_stack;
    int stack_ptr;
    int stack_capacity;
    
    // Switch memory management
    uint32_t* switchmem;       // Global switch memory table
    int switch_count;          // Number of switches processed
    int switch_offset_bits;    // Bits per switch (default 8)
    int timer_count;
    
    // Address management
    int exit_address;          // Calculated exit address for while loops
    
    // Analysis data
    int state_assignments;
    int branch_instructions;
    int jump_instructions;

    uint32_t max_jadr_val;
    uint32_t max_varsel_val;
    int max_state_val;
    int max_mask_val; // Added for compatibility with HotstateMicrocode
    int max_timersel_val; // Added for compatibility with HotstateMicrocode
    int max_timerld_val; // Added for compatibility with HotstateMicrocode
    int max_switch_sel_val; // Added for compatibility with HotstateMicrocode
    int max_switch_adr_val; // Added for compatibility with HotstateMicrocode
    int max_state_capture_val; // Added for compatibility with HotstateMicrocode
    int max_var_or_timer_val; // Added for compatibility with HotstateMicrocode
    int max_branch_val; // Added for compatibility with HotstateMicrocode
    int max_forced_jmp_val; // Added for compatibility with HotstateMicrocode
    int max_sub_val; // Added for compatibility with HotstateMicrocode
    int max_rtn_val; // Added for compatibility with HotstateMicrocode
    int var_sel_counter; // Counter for varSel values

    // Pending jump resolution
    PendingJump* pending_jumps;
    int pending_jump_count;
    int pending_jump_capacity;
} CompactMicrocode;

// Main generation function
CompactMicrocode* ast_to_compact_microcode(Node* ast_root, HardwareContext* hw_ctx);

// Output functions
void print_compact_microcode_table(CompactMicrocode* mc, FILE* output);
void print_compact_microcode_analysis(CompactMicrocode* mc, FILE* output);
//void print_hotstate_microcode_table(HotstateMicrocode* mc, FILE* output); // Moved from cfg_to_microcode.h

// Memory management
void free_compact_microcode(CompactMicrocode* mc);

#endif // AST_TO_MICROCODE_H
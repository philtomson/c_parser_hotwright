#ifndef AST_TO_MICROCODE_H
#define AST_TO_MICROCODE_H

#include "ast.h"
#include "microcode_defs.h"
#include "hw_analyzer.h"
#include <stdio.h>
#include <stdint.h>

typedef struct {
    NodeType loop_type;  // e.g., NODE_WHILE, NODE_FOR, NODE_SWITCH
    int continue_target; // Microcode address for 'continue' statements
    int break_target;    // Microcode address for 'break' statements
} LoopSwitchContext;

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

    int max_jadr_val;
    int max_varsel_val;
    int max_state_val;
} CompactMicrocode;

// Main generation function
CompactMicrocode* ast_to_compact_microcode(Node* ast_root, HardwareContext* hw_ctx);

// Output functions
void print_compact_microcode_table(CompactMicrocode* mc, FILE* output);
void print_compact_microcode_analysis(CompactMicrocode* mc, FILE* output);

// Memory management
void free_compact_microcode(CompactMicrocode* mc);

#endif // AST_TO_MICROCODE_H
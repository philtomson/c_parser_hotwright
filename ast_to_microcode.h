#ifndef AST_TO_MICROCODE_H
#define AST_TO_MICROCODE_H

#include "ast.h"
#include "hw_analyzer.h"
#include <stdio.h>
#include <stdint.h>

// Compact microcode instruction structure
typedef struct {
    uint32_t instruction_word;  // 24-bit hotstate instruction
    char* label;               // Human-readable label
    int address;               // Instruction address
    int switch_id;             // Switch ID for this instruction (-1 if not a switch)
    int is_switch;             // Flag indicating switch instruction
    int is_case;               // Flag indicating case target
} CompactInstruction;

// Compact microcode structure
typedef struct {
    CompactInstruction* instructions;
    int instruction_count;
    int instruction_capacity;
    char* function_name;
    HardwareContext* hw_ctx;
    
    // Switch memory management
    uint32_t* switchmem;       // Global switch memory table
    int switch_count;          // Number of switches processed
    int switch_offset_bits;    // Bits per switch (default 8)
    
    // Address management
    int exit_address;          // Calculated exit address for while loops
    
    // Analysis data
    int state_assignments;
    int branch_instructions;
    int jump_instructions;
} CompactMicrocode;

// Main generation function
CompactMicrocode* ast_to_compact_microcode(Node* ast_root, HardwareContext* hw_ctx);

// Output functions
void print_compact_microcode_table(CompactMicrocode* mc, FILE* output);
void print_compact_microcode_analysis(CompactMicrocode* mc, FILE* output);

// Memory management
void free_compact_microcode(CompactMicrocode* mc);

#endif // AST_TO_MICROCODE_H
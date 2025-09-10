#ifndef MICROCODE_H
#define MICROCODE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// State machine microcode instruction structure
// Based on hotstate's microcode format
typedef struct {
    uint32_t state;          // Output state assignments (bit mask)
    uint32_t mask;           // State mask for operations
    uint32_t jadr;           // Jump address (next instruction)
    uint32_t varSel;         // Variable selection for conditions
    uint32_t timerSel;       // Timer selection (unused for now)
    uint32_t timerLd;        // Timer load (unused for now)
    uint32_t switch_sel;     // Switch selection (unused for now)
    uint32_t switch_adr;     // Switch address (unused for now)
    uint32_t state_capture;  // State capture flag
    uint32_t var_or_timer;   // Variable or timer flag
    uint32_t branch;         // Branch condition flag
    uint32_t forced_jmp;     // Forced jump flag
    uint32_t sub;            // Subroutine call (unused for now)
    uint32_t rtn;            // Return flag (unused for now)
    char* label;             // Human-readable label for this instruction
} MicrocodeInstruction;

// Hardware state variable (output)
typedef struct {
    char* name;              // Variable name (e.g., "LED0")
    int state_number;        // Bit position in state word
    bool is_output;          // Always true for state variables
} StateVariable;

// Hardware input variable
typedef struct {
    char* name;              // Variable name (e.g., "a0")
    int var_number;          // Bit position in variable word
    bool is_input;           // Always true for input variables
} InputVariable;

// Complete state machine representation
typedef struct {
    MicrocodeInstruction* instructions;  // Array of microcode instructions
    int instruction_count;               // Number of instructions
    int capacity;                        // Allocated capacity
    
    StateVariable* states;               // Array of state variables
    int state_count;                     // Number of state variables
    
    InputVariable* inputs;               // Array of input variables
    int input_count;                     // Number of input variables
    
    char* function_name;                 // Name of the function (usually "main")
} StateMachine;

// Function declarations
StateMachine* create_state_machine(const char* function_name);
void add_microcode_instruction(StateMachine* sm, MicrocodeInstruction* instr);
void add_state_variable(StateMachine* sm, const char* name);
void add_input_variable(StateMachine* sm, const char* name);
void free_state_machine(StateMachine* sm);

// Microcode output functions
void print_microcode_table(StateMachine* sm, FILE* output);
void print_microcode_header(StateMachine* sm, FILE* output);
void print_state_assignments(StateMachine* sm, FILE* output);
void print_variable_mappings(StateMachine* sm, FILE* output);

#endif // MICROCODE_H
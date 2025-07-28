#include "microcode.h"
#include <stdlib.h>
#include <string.h>
//TODO appears to not be used now
// Create a new state machine
StateMachine* create_state_machine(const char* function_name) {
    StateMachine* sm = malloc(sizeof(StateMachine));
    
    // Initialize instruction array
    sm->capacity = 16;
    sm->instructions = malloc(sizeof(MicrocodeInstruction) * sm->capacity);
    sm->instruction_count = 0;
    
    // Initialize state variables array
    sm->states = malloc(sizeof(StateVariable) * 8);  // Start with 8 states
    sm->state_count = 0;
    
    // Initialize input variables array
    sm->inputs = malloc(sizeof(InputVariable) * 8);  // Start with 8 inputs
    sm->input_count = 0;
    
    // Set function name
    sm->function_name = strdup(function_name);
    
    return sm;
}

// Add a microcode instruction to the state machine
void add_microcode_instruction(StateMachine* sm, MicrocodeInstruction* instr) {
    // Resize array if needed
    if (sm->instruction_count >= sm->capacity) {
        sm->capacity *= 2;
        sm->instructions = realloc(sm->instructions, sizeof(MicrocodeInstruction) * sm->capacity);
    }
    
    // Copy the instruction
    sm->instructions[sm->instruction_count] = *instr;
    sm->instruction_count++;
}

// Add a state variable (output)
void add_state_variable(StateMachine* sm, const char* name) {
    StateVariable* state = &sm->states[sm->state_count];
    state->name = strdup(name);
    state->state_number = sm->state_count;
    state->is_output = true;
    sm->state_count++;
}

// Add an input variable
void add_input_variable(StateMachine* sm, const char* name) {
    InputVariable* input = &sm->inputs[sm->input_count];
    input->name = strdup(name);
    input->var_number = sm->input_count;
    input->is_input = true;
    sm->input_count++;
}

// Free state machine memory
void free_state_machine(StateMachine* sm) {
    if (!sm) return;
    
    // Free instruction labels
    for (int i = 0; i < sm->instruction_count; i++) {
        if (sm->instructions[i].label) {
            free(sm->instructions[i].label);
        }
    }
    free(sm->instructions);
    
    // Free state variable names
    for (int i = 0; i < sm->state_count; i++) {
        free(sm->states[i].name);
    }
    free(sm->states);
    
    // Free input variable names
    for (int i = 0; i < sm->input_count; i++) {
        free(sm->inputs[i].name);
    }
    free(sm->inputs);
    
    free(sm->function_name);
    free(sm);
}

// Print the complete microcode table in hotstate format
void print_microcode_table(StateMachine* sm, FILE* output) {
    fprintf(output, "\nState Machine Microcode derived from %s \n\n", sm->function_name);
    
    // Print the header
    print_microcode_header(sm, output);
    
    // Print each instruction
    for (int i = 0; i < sm->instruction_count; i++) {
        MicrocodeInstruction* instr = &sm->instructions[i];
        
        fprintf(output, "%x %x %x %x %x x x x x %x %x %x %x %x %x   %s\n",
            i,                          // Address (hex)
            instr->state & 0xF,         // State assignments
            instr->mask & 0xF,          // Mask
            instr->jadr & 0xF,          // Jump address
            instr->varSel & 0xF,        // Variable selection
            // x x x x (unused fields)
            instr->state_capture & 0x1, // State capture
            instr->var_or_timer & 0x1,  // Var or timer
            instr->branch & 0x1,        // Branch
            instr->forced_jmp & 0x1,    // Forced jump
            instr->sub & 0x1,           // Subroutine
            instr->rtn & 0x1,           // Return
            instr->label ? instr->label : "");
    }
    
    fprintf(output, "\n");
    
    // Print state and variable assignments
    print_state_assignments(sm, output);
    print_variable_mappings(sm, output);
}

// Print the microcode table header
void print_microcode_header(StateMachine* sm, FILE* output) {
    fprintf(output, "              s s                \n");
    fprintf(output, "              w w s     f        \n");
    fprintf(output, "a             i i t t   o        \n");
    fprintf(output, "d       v t   t t a i b r        \n");
    fprintf(output, "d s     a i t c c t m r c        \n");
    fprintf(output, "r t m j r m i h h e / a e        \n");
    fprintf(output, "e a a a S S m s a C v n j s r    \n");
    fprintf(output, "s t s d e e L e d a a c m u t    \n");
    fprintf(output, "s e k r l l d l r p r h p b n    \n");
    fprintf(output, "---------------------------------\n");
}

// Print state variable assignments
void print_state_assignments(StateMachine* sm, FILE* output) {
    fprintf(output, "State assignments\n");
    for (int i = 0; i < sm->state_count; i++) {
        fprintf(output, "state %d is %s\n", sm->states[i].state_number, sm->states[i].name);
    }
    fprintf(output, "\n");
}

// Print input variable mappings
void print_variable_mappings(StateMachine* sm, FILE* output) {
    fprintf(output, "Variable inputs\n");
    for (int i = 0; i < sm->input_count; i++) {
        fprintf(output, "var %d is %s\n", sm->inputs[i].var_number, sm->inputs[i].name);
    }
    fprintf(output, "\n");
}
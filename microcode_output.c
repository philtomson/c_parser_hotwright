#define _GNU_SOURCE  // For strdup
#include "cfg_to_microcode.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// --- Hotstate-Compatible Output Generation ---

void print_hotstate_microcode_table(HotstateMicrocode* mc, FILE* output) {
    fprintf(output, "\nState Machine Microcode derived from %s\n\n", mc->function_name);
    
    // Print the header (matching hotstate format)
    print_microcode_header(output);
    
    // Print each instruction in hotstate format
    for (int i = 0; i < mc->instruction_count; i++) {
        HotstateInstruction* instr = &mc->instructions[i];
        
        // Extract fields from packed instruction word
        uint32_t state = (instr->instruction_word & HOTSTATE_STATE_MASK) >> HOTSTATE_STATE_SHIFT;
        uint32_t mask = (instr->instruction_word & HOTSTATE_MASK_MASK) >> HOTSTATE_MASK_SHIFT;
        uint32_t jadr = (instr->instruction_word & HOTSTATE_JADR_MASK) >> HOTSTATE_JADR_SHIFT;
        uint32_t varsel = (instr->instruction_word & HOTSTATE_VARSEL_MASK) >> HOTSTATE_VARSEL_SHIFT;
        
        // Control flags
        uint32_t branch = (instr->instruction_word & HOTSTATE_BRANCH_FLAG) ? 1 : 0;
        uint32_t forced_jmp = (instr->instruction_word & HOTSTATE_FORCED_JMP) ? 1 : 0;
        uint32_t state_cap = (instr->instruction_word & HOTSTATE_STATE_CAP) ? 1 : 0;
        uint32_t var_timer = (instr->instruction_word & HOTSTATE_VAR_TIMER) ? 1 : 0;
        
        // Print in hotstate format: addr state mask jadr varsel ... flags
        fprintf(output, "%x %x %x %x %x x x x x %x %x %x %x %x %x   %s\n",
            i,                  // Address (hex)
            state & 0xF,        // State assignments
            mask & 0xF,         // Mask
            jadr & 0xF,         // Jump address
            varsel & 0xF,       // Variable selection
            // x x x x (unused fields)
            state_cap,          // State capture
            var_timer,          // Var or timer
            branch,             // Branch
            forced_jmp,         // Forced jump
            0,                  // Subroutine (unused)
            0,                  // Return (unused)
            instr->label ? instr->label : "");
    }
    
    fprintf(output, "\n");
    
    // Print state and variable assignments
    print_state_assignments(mc, output);
    print_variable_mappings(mc, output);
}

void print_microcode_header(FILE* output) {
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

void print_state_assignments(HotstateMicrocode* mc, FILE* output) {
    fprintf(output, "State assignments\n");
    for (int i = 0; i < mc->hw_ctx->state_count; i++) {
        StateVariable* state = &mc->hw_ctx->states[i];
        fprintf(output, "state %d is %s\n", state->state_number, state->name);
    }
    fprintf(output, "\n");
}

void print_variable_mappings(HotstateMicrocode* mc, FILE* output) {
    fprintf(output, "Variable inputs\n");
    for (int i = 0; i < mc->hw_ctx->input_count; i++) {
        InputVariable* input = &mc->hw_ctx->inputs[i];
        fprintf(output, "var %d is %s\n", input->input_number, input->name);
    }
    fprintf(output, "\n");
}

// --- Memory File Generation ---

void generate_smdata_mem_file(HotstateMicrocode* mc, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Error: Cannot create file '%s'\n", filename);
        return;
    }
    
    // Write each instruction as a 6-digit hex value (24-bit)
    for (int i = 0; i < mc->instruction_count; i++) {
        fprintf(file, "%06x\n", mc->instructions[i].instruction_word & 0xFFFFFF);
    }
    
    fclose(file);
    printf("Generated microcode memory file: %s\n", filename);
}

void generate_vardata_mem_file(HotstateMicrocode* mc, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Error: Cannot create file '%s'\n", filename);
        return;
    }
    
    // For now, generate a simple variable mapping
    // This would be more sophisticated in a full implementation
    for (int i = 0; i < mc->hw_ctx->input_count; i++) {
        fprintf(file, "%02x\n", i);
    }
    
    fclose(file);
    printf("Generated variable data file: %s\n", filename);
}

// --- Debug Output ---

void print_microcode_analysis(HotstateMicrocode* mc, FILE* output) {
    fprintf(output, "\n=== Microcode Analysis ===\n");
    fprintf(output, "Function: %s\n", mc->function_name);
    fprintf(output, "Total instructions: %d\n", mc->instruction_count);
    fprintf(output, "State assignments: %d\n", mc->state_assignments);
    fprintf(output, "Branch instructions: %d\n", mc->branches);
    fprintf(output, "Jump instructions: %d\n", mc->jumps);
    fprintf(output, "Basic blocks: %d\n", mc->block_count);
    
    fprintf(output, "\nHardware Resources:\n");
    fprintf(output, "State variables: %d\n", mc->hw_ctx->state_count);
    fprintf(output, "Input variables: %d\n", mc->hw_ctx->input_count);
    
    fprintf(output, "\nValidation: %s\n", 
            validate_microcode(mc) ? "PASSED" : "FAILED");
}

void print_instruction_details(HotstateInstruction* instr, FILE* output) {
    fprintf(output, "Addr %02x: %06x", instr->address, instr->instruction_word);
    
    if (instr->label) {
        fprintf(output, " (%s)", instr->label);
    }
    
    // Decode instruction fields
    uint32_t state = (instr->instruction_word & HOTSTATE_STATE_MASK) >> HOTSTATE_STATE_SHIFT;
    uint32_t mask = (instr->instruction_word & HOTSTATE_MASK_MASK) >> HOTSTATE_MASK_SHIFT;
    uint32_t jadr = (instr->instruction_word & HOTSTATE_JADR_MASK) >> HOTSTATE_JADR_SHIFT;
    uint32_t varsel = (instr->instruction_word & HOTSTATE_VARSEL_MASK) >> HOTSTATE_VARSEL_SHIFT;
    
    if (state) fprintf(output, " state=0x%x", state);
    if (mask) fprintf(output, " mask=0x%x", mask);
    if (jadr) fprintf(output, " jadr=0x%x", jadr);
    if (varsel) fprintf(output, " varsel=0x%x", varsel);
    
    if (instr->instruction_word & HOTSTATE_BRANCH_FLAG) fprintf(output, " BRANCH");
    if (instr->instruction_word & HOTSTATE_FORCED_JMP) fprintf(output, " JUMP");
    if (instr->instruction_word & HOTSTATE_STATE_CAP) fprintf(output, " CAPTURE");
    if (instr->instruction_word & HOTSTATE_VAR_TIMER) fprintf(output, " VAR");
    
    fprintf(output, "\n");
}

void print_address_mapping(HotstateMicrocode* mc, FILE* output) {
    fprintf(output, "\n=== Address Mapping ===\n");
    for (int i = 0; i < mc->block_count; i++) {
        if (i < mc->source_cfg->block_count) {
            BasicBlock* block = mc->source_cfg->blocks[i];
            fprintf(output, "Block %d -> Address 0x%02x\n", 
                    block->id, mc->block_addresses[block->id]);
        }
    }
}

// --- Utility Functions ---

char* generate_base_filename(const char* source_filename) {
    if (!source_filename) {
        return strdup("output");
    }
    
    // Find the last dot to remove extension
    const char* dot_pos = strrchr(source_filename, '.');
    const char* slash_pos = strrchr(source_filename, '/');
    
    // Start from after the last slash (or beginning if no slash)
    const char* start = slash_pos ? slash_pos + 1 : source_filename;
    
    // End at the dot (or end of string if no dot)
    size_t len = dot_pos && dot_pos > start ? (size_t)(dot_pos - start) : strlen(start);
    
    char* base = malloc(len + 1);
    strncpy(base, start, len);
    base[len] = '\0';
    
    return base;
}

void generate_all_output_files(HotstateMicrocode* mc, const char* source_filename) {
    char* base = generate_base_filename(source_filename);
    
    // Generate memory files
    char smdata_filename[256];
    char vardata_filename[256];
    
    snprintf(smdata_filename, sizeof(smdata_filename), "%s_smdata.mem", base);
    snprintf(vardata_filename, sizeof(vardata_filename), "%s_vardata.mem", base);
    
    generate_smdata_mem_file(mc, smdata_filename);
    generate_vardata_mem_file(mc, vardata_filename);
    
    free(base);
}
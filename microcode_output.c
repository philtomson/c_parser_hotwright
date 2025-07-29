#define _GNU_SOURCE  // For strdup
#include "cfg_to_microcode.h"
#include "microcode_defs.h" // Include new microcode definitions
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h> // For ceil and log2

extern int debug_mode; // Declare debug_mode as external

// Forward declarations for local functions (will be moved to header later)
static int calculate_bit_width(int max_val);
static uint64_t pack_mcode_instruction(MCode* mcode, HotstateMicrocode* mc);
static void generate_microcode_params_vh(HotstateMicrocode* mc, const char* filename);

// --- Hotstate-Compatible Output Generation ---

void print_hotstate_microcode_table(HotstateMicrocode* mc, FILE* output) {
    fprintf(output, "\nState Machine Microcode derived from %s\n\n", mc->function_name);
    
    // Print the header (matching hotstate format)
    print_microcode_header(output);
    
    // Print each instruction in hotstate format
    for (int i = 0; i < mc->instruction_count; i++) {
        Code* code_entry = &mc->instructions[i];
        MCode* mcode = &code_entry->uword.mcode;
        
        // Extract fields from MCode struct
        uint32_t state = mcode->state;
        uint32_t mask = mcode->mask;
        uint32_t jadr = mcode->jadr;
        uint32_t varsel = mcode->varSel;
        uint32_t timer_sel = mcode->timerSel;
        uint32_t timer_ld = mcode->timerLd;
        uint32_t switch_sel = mcode->switch_sel;
        uint32_t switch_adr = mcode->switch_adr;
        uint32_t state_cap = mcode->state_capture;
        uint32_t var_timer = mcode->var_or_timer;
        uint32_t branch = mcode->branch;
        uint32_t forced_jmp = mcode->forced_jmp;
        uint32_t sub = mcode->sub;
        uint32_t rtn = mcode->rtn;
        
        // Print in hotstate format: addr state mask jadr varsel unused1 unused2 switchsel switchadr flags
        fprintf(output, "%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x   %s\n",
            i,                  // Address (hex)
            state & 0xF,        // State assignments
            mask & 0xF,         // Mask
            jadr & 0xF,         // Jump address
            varsel,             // Variable selection
            timer_sel & 0xF,    // Timer selection (new)
            timer_ld & 0x1,     // Timer load (new)
            switch_sel & 0xF,   // Switch selection
            switch_adr & 0x1,   // Switch address flag
            state_cap,          // State capture
            var_timer,          // Var or timer
            branch,             // Branch
            forced_jmp,         // Forced jump
            sub,                // Subroutine
            rtn,                // Return
            code_entry->label ? code_entry->label : "");
    }
    
    fprintf(output, "\n");
    
    // Print state and variable assignments
    print_state_assignments(mc, output);
    print_variable_mappings(mc, output);
}

// Helper function to calculate minimum bit-width for a given maximum value
static int calculate_bit_width(int max_val) {
    if (max_val < 0) return 0; // Should not happen for widths
    if (max_val == 0) return 1; // A single bit is needed for value 0
    return (int)ceil(log2(max_val + 1));
}

// Function to pack an MCode struct into a uint64_t based on dynamic bit-widths
// The packing order must match the hardware's unpacking order.
// This example uses a fixed order for now, but in a real scenario,
// this order would need to be defined and consistent.
static uint64_t pack_mcode_instruction(MCode* mcode, HotstateMicrocode* mc) {
    uint64_t packed_word = 0;
    int current_shift = 0;

    // Pack fields in a defined order (LSB to MSB for simplicity, can be changed)
    // The order here should eventually match the INSTR_WIDTH calculation in Verilog
    
    // rtn (1 bit)
    printf("DEBUG: microcode_output.c: pack_mcode_instruction: debug_mode = %d\n", debug_mode);
    printf("DEBUG: pack_mcode_instruction: Packing rtn=%d (width=%d) at shift=%d\n", mcode->rtn, calculate_bit_width(mc->max_rtn_val), current_shift);
    packed_word |= ((uint64_t)mcode->rtn & ((1ULL << calculate_bit_width(mc->max_rtn_val)) - 1)) << current_shift;
    current_shift += calculate_bit_width(mc->max_rtn_val);

    // sub (1 bit)
    printf("DEBUG: pack_mcode_instruction: Packing sub=%d (width=%d) at shift=%d\n", mcode->sub, calculate_bit_width(mc->max_sub_val), current_shift);
    packed_word |= ((uint64_t)mcode->sub & ((1ULL << calculate_bit_width(mc->max_sub_val)) - 1)) << current_shift;
    current_shift += calculate_bit_width(mc->max_sub_val);

    // forced_jmp (1 bit)
    printf("DEBUG: pack_mcode_instruction: Packing forced_jmp=%d (width=%d) at shift=%d\n", mcode->forced_jmp, calculate_bit_width(mc->max_forced_jmp_val), current_shift);
    packed_word |= ((uint64_t)mcode->forced_jmp & ((1ULL << calculate_bit_width(mc->max_forced_jmp_val)) - 1)) << current_shift;
    current_shift += calculate_bit_width(mc->max_forced_jmp_val);

    // branch (1 bit)
    printf("DEBUG: pack_mcode_instruction: Packing branch=%d (width=%d) at shift=%d\n", mcode->branch, calculate_bit_width(mc->max_branch_val), current_shift);
    packed_word |= ((uint64_t)mcode->branch & ((1ULL << calculate_bit_width(mc->max_branch_val)) - 1)) << current_shift;
    current_shift += calculate_bit_width(mc->max_branch_val);

    // var_or_timer (1 bit)
    printf("DEBUG: pack_mcode_instruction: Packing var_or_timer=%d (width=%d) at shift=%d\n", mcode->var_or_timer, calculate_bit_width(mc->max_var_or_timer_val), current_shift);
    packed_word |= ((uint64_t)mcode->var_or_timer & ((1ULL << calculate_bit_width(mc->max_var_or_timer_val)) - 1)) << current_shift;
    current_shift += calculate_bit_width(mc->max_var_or_timer_val);

    // state_capture (1 bit)
    printf("DEBUG: pack_mcode_instruction: Packing state_capture=%d (width=%d) at shift=%d\n", mcode->state_capture, calculate_bit_width(mc->max_state_capture_val), current_shift);
    packed_word |= ((uint64_t)mcode->state_capture & ((1ULL << calculate_bit_width(mc->max_state_capture_val)) - 1)) << current_shift;
    current_shift += calculate_bit_width(mc->max_state_capture_val);

    // switch_adr (1 bit)
    printf("DEBUG: pack_mcode_instruction: Packing switch_adr=%d (width=%d) at shift=%d\n", mcode->switch_adr, calculate_bit_width(mc->max_switch_adr_val), current_shift);
    packed_word |= ((uint64_t)mcode->switch_adr & ((1ULL << calculate_bit_width(mc->max_switch_adr_val)) - 1)) << current_shift;
    current_shift += calculate_bit_width(mc->max_switch_adr_val);

    // switch_sel (width based on max_switch_sel_val)
    printf("DEBUG: pack_mcode_instruction: Packing switch_sel=%d (width=%d) at shift=%d\n", mcode->switch_sel, calculate_bit_width(mc->max_switch_sel_val), current_shift);
    packed_word |= ((uint64_t)mcode->switch_sel & ((1ULL << calculate_bit_width(mc->max_switch_sel_val)) - 1)) << current_shift;
    current_shift += calculate_bit_width(mc->max_switch_sel_val);

    // timerLd (1 bit)
    printf("DEBUG: pack_mcode_instruction: Packing timerLd=%d (width=%d) at shift=%d\n", mcode->timerLd, calculate_bit_width(mc->max_timerld_val), current_shift);
    packed_word |= ((uint64_t)mcode->timerLd & ((1ULL << calculate_bit_width(mc->max_timerld_val)) - 1)) << current_shift;
    current_shift += calculate_bit_width(mc->max_timerld_val);

    // timerSel (width based on max_timersel_val)
    printf("DEBUG: pack_mcode_instruction: Packing timerSel=%d (width=%d) at shift=%d\n", mcode->timerSel, calculate_bit_width(mc->max_timersel_val), current_shift);
    packed_word |= ((uint64_t)mcode->timerSel & ((1ULL << calculate_bit_width(mc->max_timersel_val)) - 1)) << current_shift;
    current_shift += calculate_bit_width(mc->max_timersel_val);

    // varSel (width based on max_varsel_val)
    printf("DEBUG: pack_mcode_instruction: Packing varSel=%d (width=%d) at shift=%d\n", mcode->varSel, calculate_bit_width(mc->max_varsel_val), current_shift);
    packed_word |= ((uint64_t)mcode->varSel & ((1ULL << calculate_bit_width(mc->max_varsel_val)) - 1)) << current_shift;
    current_shift += calculate_bit_width(mc->max_varsel_val);

    // jadr (width based on max_jadr_val)
    printf("DEBUG: pack_mcode_instruction: Packing jadr=%d (width=%d) at shift=%d\n", mcode->jadr, calculate_bit_width(mc->max_jadr_val), current_shift);
    packed_word |= ((uint64_t)mcode->jadr & ((1ULL << calculate_bit_width(mc->max_jadr_val)) - 1)) << current_shift;
    current_shift += calculate_bit_width(mc->max_jadr_val);

    // mask (width based on max_mask_val)
    printf("DEBUG: pack_mcode_instruction: Packing mask=%d (width=%d) at shift=%d\n", mcode->mask, calculate_bit_width(mc->max_mask_val), current_shift);
    packed_word |= ((uint64_t)mcode->mask & ((1ULL << calculate_bit_width(mc->max_mask_val)) - 1)) << current_shift;
    current_shift += calculate_bit_width(mc->max_mask_val);

    // state (width based on max_state_val)
    printf("DEBUG: pack_mcode_instruction: Packing state=%d (width=%d) at shift=%d\n", mcode->state, calculate_bit_width(mc->max_state_val), current_shift);
    packed_word |= ((uint64_t)mcode->state & ((1ULL << calculate_bit_width(mc->max_state_val)) - 1)) << current_shift;
    current_shift += calculate_bit_width(mc->max_state_val);

    printf("DEBUG: pack_mcode_instruction: Final packed_word = 0x%llx\n", packed_word);
    return packed_word;
}

// Function to generate the microcode_params.vh file
static void generate_microcode_params_vh(HotstateMicrocode* mc, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Error: Cannot create file '%s'\n", filename);
        return;
    }

    fprintf(file, "`ifndef MICROCODE_PARAMS_VH\n");
    fprintf(file, "`define MICROCODE_PARAMS_VH\n\n");

    fprintf(file, "localparam STATE_WIDTH = %d;\n", calculate_bit_width(mc->max_state_val));
    fprintf(file, "localparam MASK_WIDTH = %d;\n", calculate_bit_width(mc->max_mask_val));
    fprintf(file, "localparam JADR_WIDTH = %d;\n", calculate_bit_width(mc->max_jadr_val));
    fprintf(file, "localparam VARSEL_WIDTH = %d;\n", calculate_bit_width(mc->max_varsel_val));
    fprintf(file, "localparam TIMERSEL_WIDTH = %d;\n", calculate_bit_width(mc->max_timersel_val));
    fprintf(file, "localparam TIMERLD_WIDTH = %d;\n", calculate_bit_width(mc->max_timerld_val));
    fprintf(file, "localparam SWITCH_SEL_WIDTH = %d;\n", calculate_bit_width(mc->max_switch_sel_val));
    fprintf(file, "localparam SWITCH_ADR_WIDTH = %d;\n", calculate_bit_width(mc->max_switch_adr_val));
    fprintf(file, "localparam STATE_CAPTURE_WIDTH = %d;\n", calculate_bit_width(mc->max_state_capture_val));
    fprintf(file, "localparam VAR_OR_TIMER_WIDTH = %d;\n", calculate_bit_width(mc->max_var_or_timer_val));
    fprintf(file, "localparam BRANCH_WIDTH = %d;\n", calculate_bit_width(mc->max_branch_val));
    fprintf(file, "localparam FORCED_JMP_WIDTH = %d;\n", calculate_bit_width(mc->max_forced_jmp_val));
    fprintf(file, "localparam SUB_WIDTH = %d;\n", calculate_bit_width(mc->max_sub_val));
    fprintf(file, "localparam RTN_WIDTH = %d;\n", calculate_bit_width(mc->max_rtn_val));

    // Calculate total INSTR_WIDTH
    fprintf(file, "\nlocalparam INSTR_WIDTH = STATE_WIDTH + MASK_WIDTH + JADR_WIDTH + VARSEL_WIDTH + \n");
    fprintf(file, "                         TIMERSEL_WIDTH + TIMERLD_WIDTH + SWITCH_SEL_WIDTH + SWITCH_ADR_WIDTH + \n");
    fprintf(file, "                         STATE_CAPTURE_WIDTH + VAR_OR_TIMER_WIDTH + BRANCH_WIDTH + FORCED_JMP_WIDTH + \n");
    fprintf(file, "                         SUB_WIDTH + RTN_WIDTH;\n");

    fprintf(file, "\n`endif // MICROCODE_PARAMS_VH\n");
    
    fclose(file);
    printf("Generated Verilog parameter file: %s\n", filename);
}

// --- Memory File Generation ---

void generate_smdata_mem_file(HotstateMicrocode* mc, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Error: Cannot create file '%s'\n", filename);
        return;
    }
    
    // Calculate hex width based on total instruction width
    int total_instr_width = calculate_bit_width(mc->max_state_val) +
                            calculate_bit_width(mc->max_mask_val) +
                            calculate_bit_width(mc->max_jadr_val) +
                            calculate_bit_width(mc->max_varsel_val) +
                            calculate_bit_width(mc->max_timersel_val) +
                            calculate_bit_width(mc->max_timerld_val) +
                            calculate_bit_width(mc->max_switch_sel_val) +
                            calculate_bit_width(mc->max_switch_adr_val) +
                            calculate_bit_width(mc->max_state_capture_val) +
                            calculate_bit_width(mc->max_var_or_timer_val) +
                            calculate_bit_width(mc->max_branch_val) +
                            calculate_bit_width(mc->max_forced_jmp_val) +
                            calculate_bit_width(mc->max_sub_val) +
                            calculate_bit_width(mc->max_rtn_val);
 
    int hex_width = (total_instr_width + 3) / 4;
    if (hex_width == 0) hex_width = 1; // Ensure at least 1 hex digit
 
    char format_str[16];
    snprintf(format_str, sizeof(format_str), "%%0%dllx\n", hex_width); // Use %llx for uint64_t
 
    // Write each instruction with variable width (no masking needed)
    for (int i = 0; i < mc->instruction_count; i++) {
        uint64_t packed_instruction = pack_mcode_instruction(&mc->instructions[i].uword.mcode, mc);
        fprintf(file, format_str, packed_instruction);
    }
    
    fclose(file);
    printf("Generated microcode memory file: %s (width: %d hex digits, total bit width: %d)\n", filename, hex_width, total_instr_width);
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


//TODO: this needs to be filled in with actual vardata
//TODO: actuall vadata needs to be created somewhere
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

void print_instruction_details(Code* code_entry, FILE* output) {
    MCode* mcode = &code_entry->uword.mcode;
    
    fprintf(output, "Addr %02x:", code_entry->level); // Using level as address for now
    
    if (code_entry->label) {
        fprintf(output, " (%s)", code_entry->label);
    }
    
    // Decode instruction fields from MCode struct
    if (mcode->state) fprintf(output, " state=0x%x", mcode->state);
    if (mcode->mask) fprintf(output, " mask=0x%x", mcode->mask);
    if (mcode->jadr) fprintf(output, " jadr=0x%x", mcode->jadr);
    if (mcode->varSel) fprintf(output, " varSel=0x%x", mcode->varSel);
    if (mcode->timerSel) fprintf(output, " timerSel=0x%x", mcode->timerSel);
    if (mcode->timerLd) fprintf(output, " timerLd=0x%x", mcode->timerLd);
    if (mcode->switch_sel) fprintf(output, " switch_sel=0x%x", mcode->switch_sel);
    if (mcode->switch_adr) fprintf(output, " switch_adr=0x%x", mcode->switch_adr);
    if (mcode->state_capture) fprintf(output, " state_capture=0x%x", mcode->state_capture);
    if (mcode->var_or_timer) fprintf(output, " var_or_timer=0x%x", mcode->var_or_timer);
    if (mcode->branch) fprintf(output, " branch");
    if (mcode->forced_jmp) fprintf(output, " forced_jmp");
    if (mcode->sub) fprintf(output, " sub");
    if (mcode->rtn) fprintf(output, " rtn");
    
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

// Generates a full output filepath (including directory) from a source filename and a suffix
char* generate_output_filepath(const char* source_filename, const char* suffix) {
    if (!source_filename) {
        // Fallback for no source filename
        char* default_name = malloc(strlen("output") + strlen(suffix) + 1);
        sprintf(default_name, "output%s", suffix);
        return default_name;
    }
    
    // Find the last slash to determine the directory
    const char* last_slash = strrchr(source_filename, '/');
    
    // Find the last dot to remove extension
    const char* dot_pos = strrchr(source_filename, '.');

    // Determine the base name start and length
    const char* base_start = last_slash ? last_slash + 1 : source_filename;
    size_t base_len = dot_pos ? (size_t)(dot_pos - base_start) : strlen(base_start);

    // Determine the directory part
    size_t dir_len = last_slash ? (size_t)(last_slash - source_filename + 1) : 0; // +1 for the slash

    // Allocate memory for the full path
    char* full_path = malloc(dir_len + base_len + strlen(suffix) + 1);
    if (!full_path) {
        fprintf(stderr, "Error: Memory allocation failed for output filepath.\n");
        return NULL;
    }

    // Copy directory part
    if (dir_len > 0) {
        strncpy(full_path, source_filename, dir_len);
    }
    full_path[dir_len] = '\0'; // Null-terminate the directory part

    // Append base name
    strncat(full_path, base_start, base_len);

    // Append suffix
    strcat(full_path, suffix);

    return full_path;
}

// Debug print for generated file paths
void debug_print_filepaths(const char* source_filename) {
    char* smdata_filepath = generate_output_filepath(source_filename, "_smdata.mem");
    char* vardata_filepath = generate_output_filepath(source_filename, "_vardata.mem");
    char* params_filepath = generate_output_filepath(source_filename, "_params.vh");

    printf("Debug: smdata_filepath = %s\n", smdata_filepath);
    printf("Debug: vardata_filepath = %s\n", vardata_filepath);
    printf("Debug: params_filepath = %s\n", params_filepath);

    free(smdata_filepath);
    free(vardata_filepath);
    free(params_filepath);
}

void generate_all_output_files(HotstateMicrocode* mc, const char* source_filename) {
    printf("DEBUG: microcode_output.c: generate_all_output_files: debug_mode = %d\n", debug_mode);
    printf("DEBUG: generate_all_output_files: Starting output generation.\n");
    char* smdata_filepath = generate_output_filepath(source_filename, "_smdata.mem");
    char* vardata_filepath = generate_output_filepath(source_filename, "_vardata.mem");
    char* params_filepath = generate_output_filepath(source_filename, "_params.vh");

    // Add debug print here
    debug_print_filepaths(source_filename);

    if (!smdata_filepath || !vardata_filepath || !params_filepath) {
        // Handle allocation errors, already printed inside generate_output_filepath
        free(smdata_filepath);
        free(vardata_filepath);
        free(params_filepath);
        return;
    }
    
    generate_microcode_params_vh(mc, params_filepath);
    generate_smdata_mem_file(mc, smdata_filepath);
    generate_vardata_mem_file(mc, vardata_filepath);
    
    free(smdata_filepath);
    free(vardata_filepath);
    free(params_filepath);
}
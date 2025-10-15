#define _GNU_SOURCE  // For strdup
#include "cfg_to_microcode.h"
#include "microcode_defs.h" // Include new microcode definitions
#include "ast_to_microcode.h" // Include CompactMicrocode definition
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h> // For INT_MIN

// Global configuration variables
int switch_offset_bits = DEFAULT_SWITCH_OFFSET_BITS; // Default value


// --- Internal Helper Functions ---
 
static void init_hotstate_microcode(HotstateMicrocode* mc, CFG* cfg, HardwareContext* hw_ctx);
static char* generate_instruction_label(SSAInstruction* instr, BasicBlock* block);
static bool is_boolean_expression(Node* expr);
static int count_expected_instructions(CFG* cfg);

// --- Main Translation Function ---

HotstateMicrocode* cfg_to_hotstate_microcode(CFG* cfg, HardwareContext* hw_ctx) {
    if (!cfg || !hw_ctx) {
        return NULL;
    }
    
    HotstateMicrocode* mc = create_hotstate_microcode(cfg, hw_ctx);
    
    // Phase 1: Build address mapping for all blocks
    build_address_mapping(mc);
    
    // Phase 2: Translate each basic block to microcode
    for (int i = 0; i < cfg->block_count; i++) {
        translate_basic_block(mc, cfg->blocks[i]);
    }
    
    // Phase 3: Resolve jump addresses
    resolve_jump_addresses(mc);
    
    // Phase 4: Validate the generated microcode
    if (!validate_microcode(mc)) {
        printf("Warning: Generated microcode failed validation\n");
    }
    
    return mc;
}

// --- Block Translation ---

void translate_basic_block(HotstateMicrocode* mc, BasicBlock* block) {
    if (!block) return;
    
    // Record the starting address for this block
    mc->block_addresses[block->id] = mc->instruction_count;
    
    // Translate phi nodes (if any)
    translate_phi_nodes(mc, block);
    
    // Translate regular SSA instructions
    translate_instructions(mc, block);
    
    // Translate control flow (branches/jumps)
    translate_control_flow(mc, block);
}

void translate_phi_nodes(HotstateMicrocode* mc, BasicBlock* block) {
    if (!block->phi_nodes) return;
    
    // For now, phi nodes are handled by state assignments
    // In a more sophisticated implementation, we would need to
    // generate conditional assignments based on predecessor blocks
    for (int i = 0; i < block->phi_nodes->count; i++) {
        PhiNode* phi = block->phi_nodes->items[i];
        
        // Generate a comment instruction for phi nodes
        char label[256];
        snprintf(label, sizeof(label), "phi: %s", 
                ssa_value_to_string(phi->dest));
        
        // Populate MCode struct for NOP
        MCode nop_mcode = {0}; // All fields to 0
        nop_mcode.forced_jmp = 1; // NOP usually means just continue
        add_hotstate_instruction(mc, nop_mcode, label, block);
        print_debug("DEBUG: translate_phi_nodes: Added NOP MCode (forced_jmp: %d)\n", nop_mcode.forced_jmp);
    }
}

void translate_instructions(HotstateMicrocode* mc, BasicBlock* block) {
    if (!block->instructions) return;
    
    for (int i = 0; i < block->instructions->count; i++) {
        SSAInstruction* instr = block->instructions->items[i];
        MCode mcode = {0}; // Initialize all fields to 0
        char* label = generate_instruction_label(instr, block);
        
        switch (instr->type) {
            case SSA_ASSIGN:
                if (is_state_assignment(instr, mc->hw_ctx)) {
                    // For now, encode_state_assignment returns uint32_t.
                    // We need to convert it to MCode. This is a temporary solution.
                    // Later, encode_state_assignment should directly return MCode.
                    mcode = encode_state_assignment(instr, mc->hw_ctx);

                    mc->state_assignments++;
                } else {
                    mcode.forced_jmp = 1; // NOP
                }
                break;
                
            case SSA_BINARY_OP:
                mcode.forced_jmp = 1; // NOP
                break;
                
            case SSA_CALL:
                mcode.forced_jmp = 1; // NOP
                break;
                
            default:
                mcode.forced_jmp = 1; // NOP
                break;
        }
        
        add_hotstate_instruction(mc, mcode, label, block);
        print_debug("DEBUG: translate_instructions: Added MCode (state: %d, mask: %d, jadr: %d, varSel: %d, timerSel: %d, timerLd: %d, switch_sel: %d, switch_adr: %d, state_capture: %d, var_or_timer: %d, branch: %d, forced_jmp: %d, sub: %d, rtn: %d)\n",
               mcode.state, mcode.mask, mcode.jadr, mcode.varSel, mcode.timerSel, mcode.timerLd, mcode.switch_sel, mcode.switch_adr, mcode.state_capture, mcode.var_or_timer, mcode.branch, mcode.forced_jmp, mcode.sub, mcode.rtn);
        free(label);
    }
}

void translate_control_flow(HotstateMicrocode* mc, BasicBlock* block) {
    if (block->successor_count == 0) {
        // Terminal block - add halt/return instruction
        MCode halt_mcode = {0};
        halt_mcode.forced_jmp = 1; // Or some other halt instruction
        add_hotstate_instruction(mc, halt_mcode, "halt", block);
        print_debug("DEBUG: translate_control_flow: Added HALT MCode (forced_jmp: %d)\n", halt_mcode.forced_jmp);
        return;
    }
    
    if (block->successor_count == 1) {
        // Unconditional jump to successor
        BasicBlock* target = block->successors[0];
        int target_addr = get_block_address(mc, target);
        // Temporary: encode_unconditional_jump still returns uint32_t
        MCode jump_mcode = encode_unconditional_jump(target_addr);
        
        char label[64];
        snprintf(label, sizeof(label), "jump -> block_%d", target->id);
        add_hotstate_instruction(mc, jump_mcode, label, block);
        mc->jumps++;
        print_debug("DEBUG: translate_control_flow: Added JUMP MCode (jadr: %d, forced_jmp: %d)\n", jump_mcode.jadr, jump_mcode.forced_jmp);
        
    } else if (block->successor_count == 2) {
        // Conditional branch - need to find the branch condition
        BasicBlock* true_target = block->successors[0];
        BasicBlock* false_target = block->successors[1];
        
        // Look for branch instruction in the block
        SSAInstruction* branch_instr = NULL;
        if (block->instructions && block->instructions->count > 0) {
            SSAInstruction* last_instr = block->instructions->items[block->instructions->count - 1];
            if (last_instr->type == SSA_BRANCH) {
                branch_instr = last_instr;
            }
        }
        
        if (branch_instr) {
            int true_addr = get_block_address(mc, true_target);
            int false_addr = get_block_address(mc, false_target);
            // Temporary: encode_conditional_branch still returns uint32_t
            MCode branch_mcode = encode_conditional_branch(branch_instr, mc->hw_ctx, true_addr, false_addr);
            
            char label[128];
            snprintf(label, sizeof(label), "branch -> block_%d, block_%d",
                    true_target->id, false_target->id);
            add_hotstate_instruction(mc, branch_mcode, label, block);
            mc->branches++;
            print_debug("DEBUG: translate_control_flow: Added BRANCH MCode (jadr: %d, varSel: %d, branch: %d, var_or_timer: %d)\n",
                   branch_mcode.jadr, branch_mcode.varSel, branch_mcode.branch, branch_mcode.var_or_timer);
            
            // Add unconditional jump for false case (next instruction)
            MCode false_jump_mcode = encode_unconditional_jump(false_addr);

            snprintf(label, sizeof(label), "false -> block_%d", false_target->id);
            add_hotstate_instruction(mc, false_jump_mcode, label, block);
            mc->jumps++;
            print_debug("DEBUG: translate_control_flow: Added FALSE JUMP MCode (jadr: %d, forced_jmp: %d)\n", false_jump_mcode.jadr, false_jump_mcode.forced_jmp);
        } else {
            // No explicit branch instruction - generate default behavior
            int target_addr = get_block_address(mc, true_target);
            MCode jump_mcode = encode_unconditional_jump(target_addr);
            add_hotstate_instruction(mc, jump_mcode, "default_jump", block);
            mc->jumps++;
            print_debug("DEBUG: translate_control_flow: Added DEFAULT JUMP MCode (jadr: %d, forced_jmp: %d)\n", jump_mcode.jadr, jump_mcode.forced_jmp);
        }
    }
}

// --- Instruction Encoding ---

MCode encode_state_assignment(SSAInstruction* instr, HardwareContext* hw_ctx) {
    MCode mcode = {0}; // Initialize all fields to 0
    
    // Get the state bit to set
    int state_bit = get_state_bit_from_ssa_value(instr->dest, hw_ctx);
    if (state_bit >= 0) {
        // Set state and mask fields
        mcode.state = (1 << state_bit);
        mcode.mask = (1 << state_bit);
        
        // Set forced jump flag (continue to next instruction)
        mcode.forced_jmp = 1;
        
        // Jump address will be resolved later
        mcode.jadr = 0;
    }
    
    return mcode;
}

MCode encode_conditional_branch(SSAInstruction* instr, HardwareContext* hw_ctx, int true_addr, int false_addr) {
    MCode mcode = {0}; // Initialize all fields to 0
    
    // Get input variable for condition
    int input_num = get_input_number_from_ssa_value(instr->data.branch_data.condition, hw_ctx);
    if (input_num >= 0) {
        // Set variable selection
        mcode.varSel = input_num;
        
        // Set jump address (true branch)
        mcode.jadr = (true_addr & 0xF);
        
        // Set branch and variable flags
        mcode.branch = 1;
        mcode.var_or_timer = 1;
    }
    
    return mcode;
}

MCode encode_unconditional_jump(int target_addr) {
    MCode mcode = {0}; // Initialize all fields to 0
    
    // Set jump address
    mcode.jadr = (target_addr & 0xF);
    
    // Set forced jump flag
    mcode.forced_jmp = 1;
    
    return mcode;
}

MCode encode_nop_instruction() {
    MCode mcode = {0}; // Initialize all fields to 0
    // NOP: no state changes, no jumps, just continue
    mcode.forced_jmp = 1; // Continue to next instruction
    return mcode;
}

// --- SSA Analysis ---

bool is_state_assignment(SSAInstruction* instr, HardwareContext* hw_ctx) {
    if (instr->type != SSA_ASSIGN || !instr->dest) {
        return false;
    }
    
    return get_state_bit_from_ssa_value(instr->dest, hw_ctx) >= 0;
}

bool is_input_reference(SSAValue* value, HardwareContext* hw_ctx) {
    return get_input_number_from_ssa_value(value, hw_ctx) >= 0;
}

int get_state_bit_from_ssa_value(SSAValue* value, HardwareContext* hw_ctx) {
    if (!value || value->type != SSA_VAR) {
        return -1;
    }
    
    return get_state_number_by_name(hw_ctx, value->data.var.base_name);
}

int get_input_number_from_ssa_value(SSAValue* value, HardwareContext* hw_ctx) {
    if (!value || value->type != SSA_VAR) {
        return -1;
    }
    
    return get_input_number_by_name(hw_ctx, value->data.var.base_name);
}

// --- Address Management ---

void build_address_mapping(HotstateMicrocode* mc) {
    mc->block_addresses = calloc(mc->block_count, sizeof(int));
    
    // Pre-calculate approximate instruction count per block
    int estimated_addr = 0;
    for (int i = 0; i < mc->source_cfg->block_count; i++) {
        BasicBlock* block = mc->source_cfg->blocks[i];
        mc->block_addresses[block->id] = estimated_addr;
        
        // Estimate instructions per block
        int block_instructions = 1; // At least one instruction per block
        if (block->instructions) {
            block_instructions += block->instructions->count;
        }
        if (block->phi_nodes && block->phi_nodes->count > 0) {
            block_instructions += block->phi_nodes->count;
        }
        if (block->successor_count > 1) {
            block_instructions += 2; // Branch + jump
        } else if (block->successor_count == 1) {
            block_instructions += 1; // Jump
        }
        
        estimated_addr += block_instructions;
    }
}

void resolve_jump_addresses(HotstateMicrocode* mc) {
    // Update jump addresses in generated instructions
    for (int i = 0; i < mc->instruction_count; i++) {
        Code* code_entry = &mc->instructions[i];
        MCode* mcode = &code_entry->uword.mcode;
        
        // Check if this is a jump instruction that needs address resolution
        if (mcode->branch || mcode->forced_jmp) {
            // For now, addresses are already set during encoding
            // In a more sophisticated implementation, we would update them here
        }
    }
}

int get_block_address(HotstateMicrocode* mc, BasicBlock* block) {
    if (!block || block->id >= mc->block_count) {
        return 0;
    }
    return mc->block_addresses[block->id];
}

// --- Instruction Management ---

void add_hotstate_instruction(HotstateMicrocode* mc, MCode mcode_val, const char* label, BasicBlock* source_block) {
    if (mc->instruction_count >= mc->instruction_capacity) {
        resize_instruction_array(mc);
    }
    
    Code* code_entry = &mc->instructions[mc->instruction_count];
    code_entry->uword.mcode = mcode_val;
    code_entry->level = 0; // Default level for now
    code_entry->label = label ? strdup(label) : NULL;
    // The address and source_block can be stored as metadata in Code struct if needed
    // For now, we rely on the instruction_count as address.
    
    mc->instruction_count++;


    // Update max_val fields for dynamic bit-width calculation
    if (mcode_val.state > mc->max_state_val) mc->max_state_val = mcode_val.state;
    if (mcode_val.mask > mc->max_mask_val) mc->max_mask_val = mcode_val.mask;
    if (mcode_val.jadr > mc->max_jadr_val) mc->max_jadr_val = mcode_val.jadr;
    if (mcode_val.varSel > mc->max_varsel_val) mc->max_varsel_val = mcode_val.varSel;
    if (mcode_val.timerSel > mc->max_timersel_val) mc->max_timersel_val = mcode_val.timerSel;
    if (mcode_val.timerLd > mc->max_timerld_val) mc->max_timerld_val = mcode_val.timerLd;
    if (mcode_val.switch_sel > mc->max_switch_sel_val) mc->max_switch_sel_val = mcode_val.switch_sel;
    if (mcode_val.switch_adr > mc->max_switch_adr_val) mc->max_switch_adr_val = mcode_val.switch_adr;
    if (mcode_val.state_capture > mc->max_state_capture_val) mc->max_state_capture_val = mcode_val.state_capture;
    if (mcode_val.var_or_timer > mc->max_var_or_timer_val) mc->max_var_or_timer_val = mcode_val.var_or_timer;
    if (mcode_val.branch > mc->max_branch_val) mc->max_branch_val = mcode_val.branch;
    if (mcode_val.forced_jmp > mc->max_forced_jmp_val) mc->max_forced_jmp_val = mcode_val.forced_jmp;
    if (mcode_val.sub > mc->max_sub_val) mc->max_sub_val = mcode_val.sub;
    if (mcode_val.rtn > mc->max_rtn_val) mc->max_rtn_val = mcode_val.rtn;
}

void resize_instruction_array(HotstateMicrocode* mc) {
    mc->instruction_capacity *= 2;
    mc->instructions = realloc(mc->instructions, 
                              sizeof(Code) * mc->instruction_capacity);
}

// --- Helper Functions ---

static char* generate_instruction_label(SSAInstruction* instr, BasicBlock* block) {
    char* label = malloc(128);
    
    switch (instr->type) {
        case SSA_ASSIGN:
            if (instr->dest) {
                char* dest_str = ssa_value_to_string(instr->dest);
                snprintf(label, 128, "%s = ...", dest_str);
                free(dest_str);
            } else {
                snprintf(label, 128, "assign");
            }
            break;
            
        case SSA_BRANCH:
            snprintf(label, 128, "branch");
            break;
            
        case SSA_JUMP:
            snprintf(label, 128, "jump");
            break;
            
        default:
            snprintf(label, 128, "instr_%d", instr->type);
            break;
    }
    
    return label;
}

// --- Memory Management ---

HotstateMicrocode* create_hotstate_microcode(CFG* cfg, HardwareContext* hw_ctx) {
    HotstateMicrocode* mc = malloc(sizeof(HotstateMicrocode));
    init_hotstate_microcode(mc, cfg, hw_ctx);
    return mc;
}

void init_hotstate_microcode(HotstateMicrocode* mc, CFG* cfg, HardwareContext* hw_ctx) {
    print_debug("DEBUG: cfg_to_microcode.c: init_hotstate_microcode: debug_mode = %d\n", debug_mode);
    mc->instruction_capacity = count_expected_instructions(cfg);
    mc->instructions = malloc(sizeof(Code) * mc->instruction_capacity);
    mc->instruction_count = 0;
    
    mc->hw_ctx = hw_ctx;
    mc->source_cfg = cfg;
    mc->function_name = cfg->function_name ? strdup(cfg->function_name) : strdup("main");
    
    mc->block_addresses = NULL;
    mc->block_count = cfg->block_count;
    
    mc->state_assignments = 0;
    mc->branches = 0;
    mc->jumps = 0;

    // Initialize max_val fields to INT_MIN or 0 for single-bit flags
    mc->max_state_val = INT_MIN;
    mc->max_mask_val = INT_MIN;
    mc->max_jadr_val = INT_MIN;
    mc->max_varsel_val = INT_MIN;
    mc->max_timersel_val = INT_MIN;
    mc->max_timerld_val = INT_MIN;
    mc->max_switch_sel_val = INT_MIN;
    mc->max_switch_adr_val = INT_MIN;
    mc->max_state_capture_val = INT_MIN;
    mc->max_var_or_timer_val = INT_MIN;
    mc->max_branch_val = INT_MIN;
    mc->max_forced_jmp_val = INT_MIN;
    mc->max_sub_val = INT_MIN;
    mc->max_rtn_val = INT_MIN;
}

static int count_expected_instructions(CFG* cfg) {
    int count = 0;
    for (int i = 0; i < cfg->block_count; i++) {
        BasicBlock* block = cfg->blocks[i];
        count += 2; // Minimum per block
        if (block->instructions) {
            count += block->instructions->count;
        }
        if (block->phi_nodes) {
            count += block->phi_nodes->count;
        }
    }
    return count > 16 ? count : 16; // Minimum allocation
}

void free_hotstate_microcode(HotstateMicrocode* mc) {
    if (!mc) return;
    
    // Free instruction labels
    for (int i = 0; i < mc->instruction_count; i++) {
        free(mc->instructions[i].label);
    }
    free(mc->instructions);
    
    free(mc->block_addresses);
    free(mc->function_name);
    free(mc);
}

// --- Validation ---

bool validate_microcode(HotstateMicrocode* mc) {
    return check_address_bounds(mc) && check_variable_references(mc);
}

bool check_address_bounds(HotstateMicrocode* mc) {
    // Check that all jump addresses are within bounds
    for (int i = 0; i < mc->instruction_count; i++) {
        MCode* mcode = &mc->instructions[i].uword.mcode;
        if (mcode->branch || mcode->forced_jmp) {
            // Need to calculate the actual jump address from MCode fields if it's dynamic
            // For now, assuming jadr is the direct target address
            int addr = mcode->jadr;
            if (addr >= mc->instruction_count) {
                return false;
            }
        }
    }
    return true;
}

bool check_variable_references(HotstateMicrocode* mc) {
    // Check that all variable references are valid
    for (int i = 0; i < mc->instruction_count; i++) {
        MCode* mcode = &mc->instructions[i].uword.mcode;
        if (mcode->branch) {
            int var_sel = mcode->varSel;
            if (var_sel >= mc->hw_ctx->input_count) {
                return false;
            }
        }
    }
    return true;
}
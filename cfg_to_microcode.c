#define _GNU_SOURCE  // For strdup
#include "cfg_to_microcode.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
        
        add_hotstate_instruction(mc, encode_nop_instruction(), label, block);
    }
}

void translate_instructions(HotstateMicrocode* mc, BasicBlock* block) {
    if (!block->instructions) return;
    
    for (int i = 0; i < block->instructions->count; i++) {
        SSAInstruction* instr = block->instructions->items[i];
        uint32_t instruction_word = 0;
        char* label = generate_instruction_label(instr, block);
        
        switch (instr->type) {
            case SSA_ASSIGN:
                if (is_state_assignment(instr, mc->hw_ctx)) {
                    instruction_word = encode_state_assignment(instr, mc->hw_ctx);
                    mc->state_assignments++;
                } else {
                    instruction_word = encode_nop_instruction();
                }
                break;
                
            case SSA_BINARY_OP:
                // For now, treat as NOP - complex expressions need decomposition
                instruction_word = encode_nop_instruction();
                break;
                
            case SSA_CALL:
                // Function calls not supported in hardware - treat as NOP
                instruction_word = encode_nop_instruction();
                break;
                
            default:
                instruction_word = encode_nop_instruction();
                break;
        }
        
        add_hotstate_instruction(mc, instruction_word, label, block);
        free(label);
    }
}

void translate_control_flow(HotstateMicrocode* mc, BasicBlock* block) {
    if (block->successor_count == 0) {
        // Terminal block - add halt/return instruction
        add_hotstate_instruction(mc, encode_nop_instruction(), "halt", block);
        return;
    }
    
    if (block->successor_count == 1) {
        // Unconditional jump to successor
        BasicBlock* target = block->successors[0];
        int target_addr = get_block_address(mc, target);
        uint32_t jump_instr = encode_unconditional_jump(target_addr);
        
        char label[64];
        snprintf(label, sizeof(label), "jump -> block_%d", target->id);
        add_hotstate_instruction(mc, jump_instr, label, block);
        mc->jumps++;
        
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
            uint32_t branch_word = encode_conditional_branch(branch_instr, mc->hw_ctx, true_addr, false_addr);
            
            char label[128];
            snprintf(label, sizeof(label), "branch -> block_%d, block_%d", 
                    true_target->id, false_target->id);
            add_hotstate_instruction(mc, branch_word, label, block);
            mc->branches++;
            
            // Add unconditional jump for false case (next instruction)
            uint32_t false_jump = encode_unconditional_jump(false_addr);
            snprintf(label, sizeof(label), "false -> block_%d", false_target->id);
            add_hotstate_instruction(mc, false_jump, label, block);
            mc->jumps++;
        } else {
            // No explicit branch instruction - generate default behavior
            int target_addr = get_block_address(mc, true_target);
            uint32_t jump_instr = encode_unconditional_jump(target_addr);
            add_hotstate_instruction(mc, jump_instr, "default_jump", block);
            mc->jumps++;
        }
    }
}

// --- Instruction Encoding ---

uint32_t encode_state_assignment(SSAInstruction* instr, HardwareContext* hw_ctx) {
    uint32_t instruction = 0;
    
    // Get the state bit to set
    int state_bit = get_state_bit_from_ssa_value(instr->dest, hw_ctx);
    if (state_bit >= 0) {
        // Set state and mask fields
        instruction |= (1 << state_bit) << HOTSTATE_STATE_SHIFT;
        instruction |= (1 << state_bit) << HOTSTATE_MASK_SHIFT;
        
        // Set forced jump flag (continue to next instruction)
        instruction |= HOTSTATE_FORCED_JMP;
        
        // Jump address will be resolved later
        instruction |= 0 << HOTSTATE_JADR_SHIFT;
    }
    
    return instruction;
}

uint32_t encode_conditional_branch(SSAInstruction* instr, HardwareContext* hw_ctx, int true_addr, int false_addr) {
    uint32_t instruction = 0;
    
    // Get input variable for condition
    int input_num = get_input_number_from_ssa_value(instr->data.branch_data.condition, hw_ctx);
    if (input_num >= 0) {
        // Set variable selection
        instruction |= input_num << HOTSTATE_VARSEL_SHIFT;
        
        // Set jump address (true branch)
        instruction |= (true_addr & 0xF) << HOTSTATE_JADR_SHIFT;
        
        // Set branch and variable flags
        instruction |= HOTSTATE_BRANCH_FLAG;
        instruction |= HOTSTATE_VAR_TIMER;
    }
    
    return instruction;
}

uint32_t encode_unconditional_jump(int target_addr) {
    uint32_t instruction = 0;
    
    // Set jump address
    instruction |= (target_addr & 0xF) << HOTSTATE_JADR_SHIFT;
    
    // Set forced jump flag
    instruction |= HOTSTATE_FORCED_JMP;
    
    return instruction;
}

uint32_t encode_nop_instruction() {
    // NOP: no state changes, no jumps, just continue
    return HOTSTATE_FORCED_JMP; // Continue to next instruction
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
        HotstateInstruction* instr = &mc->instructions[i];
        
        // Check if this is a jump instruction that needs address resolution
        if (instr->instruction_word & (HOTSTATE_BRANCH_FLAG | HOTSTATE_FORCED_JMP)) {
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

void add_hotstate_instruction(HotstateMicrocode* mc, uint32_t instruction_word, const char* label, BasicBlock* source_block) {
    if (mc->instruction_count >= mc->instruction_capacity) {
        resize_instruction_array(mc);
    }
    
    HotstateInstruction* instr = &mc->instructions[mc->instruction_count];
    instr->instruction_word = instruction_word;
    instr->label = label ? strdup(label) : NULL;
    instr->address = mc->instruction_count;
    instr->source_block = source_block;
    
    mc->instruction_count++;
}

void resize_instruction_array(HotstateMicrocode* mc) {
    mc->instruction_capacity *= 2;
    mc->instructions = realloc(mc->instructions, 
                              sizeof(HotstateInstruction) * mc->instruction_capacity);
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

static void init_hotstate_microcode(HotstateMicrocode* mc, CFG* cfg, HardwareContext* hw_ctx) {
    mc->instruction_capacity = count_expected_instructions(cfg);
    mc->instructions = malloc(sizeof(HotstateInstruction) * mc->instruction_capacity);
    mc->instruction_count = 0;
    
    mc->hw_ctx = hw_ctx;
    mc->source_cfg = cfg;
    mc->function_name = cfg->function_name ? strdup(cfg->function_name) : strdup("main");
    
    mc->block_addresses = NULL;
    mc->block_count = cfg->block_count;
    
    mc->state_assignments = 0;
    mc->branches = 0;
    mc->jumps = 0;
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
        uint32_t instr = mc->instructions[i].instruction_word;
        if (instr & (HOTSTATE_BRANCH_FLAG | HOTSTATE_FORCED_JMP)) {
            int addr = (instr & HOTSTATE_JADR_MASK) >> HOTSTATE_JADR_SHIFT;
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
        uint32_t instr = mc->instructions[i].instruction_word;
        if (instr & HOTSTATE_BRANCH_FLAG) {
            int var_sel = (instr & HOTSTATE_VARSEL_MASK) >> HOTSTATE_VARSEL_SHIFT;
            if (var_sel >= mc->hw_ctx->input_count) {
                return false;
            }
        }
    }
    return true;
}
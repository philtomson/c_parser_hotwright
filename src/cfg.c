#define _GNU_SOURCE  // For strdup
#include "cfg.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// --- CFG Creation Functions ---

CFG* create_cfg(const char* function_name) {
    CFG* cfg = (CFG*)malloc(sizeof(CFG));
    if (!cfg) return NULL;
    
    cfg->entry = NULL;
    cfg->exit = NULL;
    cfg->blocks = NULL;
    cfg->block_count = 0;
    cfg->block_capacity = 0;
    cfg->next_block_id = 0;
    cfg->function_name = function_name ? strdup(function_name) : NULL;
    cfg->current_loop_header = NULL;
    cfg->current_loop_exit = NULL;
    
    return cfg;
}

BasicBlock* create_basic_block(CFG* cfg, const char* label) {
    BasicBlock* block = (BasicBlock*)malloc(sizeof(BasicBlock));
    if (!block) return NULL;
    
    block->id = cfg->next_block_id++;
    block->instructions = create_instruction_list();
    block->successors = NULL;
    block->successor_count = 0;
    block->successor_capacity = 0;
    block->predecessors = NULL;
    block->predecessor_count = 0;
    block->predecessor_capacity = 0;
    block->label = label ? strdup(label) : NULL;
    
    // SSA-specific fields
    block->phi_nodes = create_phi_node_list();
    block->idom = NULL;
    block->dom_frontier = NULL;
    block->dom_frontier_count = 0;
    block->dom_frontier_capacity = 0;
    
    // For CFG construction
    block->visited = false;
    block->post_order_num = -1;
    
    // Add to CFG
    add_block_to_cfg(cfg, block);
    
    return block;
}

void add_block_to_cfg(CFG* cfg, BasicBlock* block) {
    if (cfg->block_count >= cfg->block_capacity) {
        int new_capacity = cfg->block_capacity == 0 ? 8 : cfg->block_capacity * 2;
        BasicBlock** new_blocks = (BasicBlock**)realloc(cfg->blocks, 
                                                        new_capacity * sizeof(BasicBlock*));
        if (!new_blocks) return;
        cfg->blocks = new_blocks;
        cfg->block_capacity = new_capacity;
    }
    cfg->blocks[cfg->block_count++] = block;
}

void add_edge(BasicBlock* from, BasicBlock* to) {
    if (!from || !to) return;
    
    // Add 'to' to successors of 'from'
    if (from->successor_count >= from->successor_capacity) {
        int new_capacity = from->successor_capacity == 0 ? 4 : from->successor_capacity * 2;
        BasicBlock** new_successors = (BasicBlock**)realloc(from->successors,
                                                           new_capacity * sizeof(BasicBlock*));
        if (!new_successors) return;
        from->successors = new_successors;
        from->successor_capacity = new_capacity;
    }
    from->successors[from->successor_count++] = to;
    
    // Add 'from' to predecessors of 'to'
    if (to->predecessor_count >= to->predecessor_capacity) {
        int new_capacity = to->predecessor_capacity == 0 ? 4 : to->predecessor_capacity * 2;
        BasicBlock** new_predecessors = (BasicBlock**)realloc(to->predecessors,
                                                             new_capacity * sizeof(BasicBlock*));
        if (!new_predecessors) return;
        to->predecessors = new_predecessors;
        to->predecessor_capacity = new_capacity;
    }
    to->predecessors[to->predecessor_count++] = from;
}

void remove_edge(BasicBlock* from, BasicBlock* to) {
    if (!from || !to) return;
    
    // Remove 'to' from successors of 'from'
    for (int i = 0; i < from->successor_count; i++) {
        if (from->successors[i] == to) {
            // Shift remaining elements
            for (int j = i; j < from->successor_count - 1; j++) {
                from->successors[j] = from->successors[j + 1];
            }
            from->successor_count--;
            break;
        }
    }
    
    // Remove 'from' from predecessors of 'to'
    for (int i = 0; i < to->predecessor_count; i++) {
        if (to->predecessors[i] == from) {
            // Shift remaining elements
            for (int j = i; j < to->predecessor_count - 1; j++) {
                to->predecessors[j] = to->predecessors[j + 1];
            }
            to->predecessor_count--;
            break;
        }
    }
}

void free_cfg(CFG* cfg) {
    if (!cfg) return;
    
    // Free all blocks
    for (int i = 0; i < cfg->block_count; i++) {
        free_basic_block(cfg->blocks[i]);
    }
    free(cfg->blocks);
    
    // Free function name
    if (cfg->function_name) {
        free(cfg->function_name);
    }
    
    free(cfg);
}

void free_basic_block(BasicBlock* block) {
    if (!block) return;
    
    // Free instructions
    free_instruction_list(block->instructions);
    
    // Free phi nodes
    free_phi_node_list(block->phi_nodes);
    
    // Free arrays
    free(block->successors);
    free(block->predecessors);
    free(block->dom_frontier);
    
    // Free label
    if (block->label) {
        free(block->label);
    }
    
    free(block);
}

// --- SSA Value Creation ---

SSAValue* create_ssa_var(const char* base_name, int version) {
    SSAValue* value = (SSAValue*)malloc(sizeof(SSAValue));
    if (!value) return NULL;
    
    value->type = SSA_VAR;
    value->data.var.base_name = strdup(base_name);
    value->data.var.version = version;
    
    return value;
}

SSAValue* create_ssa_const(int const_value) {
    SSAValue* value = (SSAValue*)malloc(sizeof(SSAValue));
    if (!value) return NULL;
    
    value->type = SSA_CONST;
    value->data.const_value = const_value;
    
    return value;
}

SSAValue* create_ssa_temp(int temp_id) {
    SSAValue* value = (SSAValue*)malloc(sizeof(SSAValue));
    if (!value) return NULL;
    
    value->type = SSA_TEMP;
    value->data.temp_id = temp_id;
    
    return value;
}

SSAValue* copy_ssa_value(SSAValue* value) {
    if (!value) return NULL;
    
    switch (value->type) {
        case SSA_VAR:
            return create_ssa_var(value->data.var.base_name, value->data.var.version);
        case SSA_CONST:
            return create_ssa_const(value->data.const_value);
        case SSA_TEMP:
            return create_ssa_temp(value->data.temp_id);
        default:
            return NULL;
    }
}

void free_ssa_value(SSAValue* value) {
    if (!value) return;
    
    if (value->type == SSA_VAR && value->data.var.base_name) {
        free(value->data.var.base_name);
    }
    
    free(value);
}

char* ssa_value_to_string(SSAValue* value) {
    if (!value) return strdup("null");
    
    char* result = (char*)malloc(256);
    if (!result) return NULL;
    
    switch (value->type) {
        case SSA_VAR:
            snprintf(result, 256, "%s_%d", value->data.var.base_name, value->data.var.version);
            break;
        case SSA_CONST:
            snprintf(result, 256, "%d", value->data.const_value);
            break;
        case SSA_TEMP:
            snprintf(result, 256, "t%d", value->data.temp_id);
            break;
        default:
            snprintf(result, 256, "unknown");
            break;
    }
    
    return result;
}

// --- SSA Instruction Creation ---

SSAInstruction* create_ssa_assign(SSAValue* dest, SSAValue* src) {
    SSAInstruction* inst = (SSAInstruction*)malloc(sizeof(SSAInstruction));
    if (!inst) return NULL;
    
    inst->type = SSA_ASSIGN;
    inst->dest = dest;
    inst->operands = (SSAValue**)malloc(sizeof(SSAValue*));
    inst->operands[0] = src;
    inst->operand_count = 1;
    
    return inst;
}

SSAInstruction* create_ssa_binary_op(SSAValue* dest, TokenType op, SSAValue* left, SSAValue* right) {
    SSAInstruction* inst = (SSAInstruction*)malloc(sizeof(SSAInstruction));
    if (!inst) return NULL;
    
    inst->type = SSA_BINARY_OP;
    inst->dest = dest;
    inst->operands = (SSAValue**)malloc(2 * sizeof(SSAValue*));
    inst->operands[0] = left;
    inst->operands[1] = right;
    inst->operand_count = 2;
    inst->data.op_data.op = op;
    
    return inst;
}

SSAInstruction* create_ssa_unary_op(SSAValue* dest, TokenType op, SSAValue* operand) {
    SSAInstruction* inst = (SSAInstruction*)malloc(sizeof(SSAInstruction));
    if (!inst) return NULL;
    
    inst->type = SSA_UNARY_OP;
    inst->dest = dest;
    inst->operands = (SSAValue**)malloc(sizeof(SSAValue*));
    inst->operands[0] = operand;
    inst->operand_count = 1;
    inst->data.op_data.op = op;
    
    return inst;
}

SSAInstruction* create_ssa_call(SSAValue* dest, const char* func_name, SSAValue** args, int arg_count) {
    SSAInstruction* inst = (SSAInstruction*)malloc(sizeof(SSAInstruction));
    if (!inst) return NULL;
    
    inst->type = SSA_CALL;
    inst->dest = dest;
    inst->operands = (SSAValue**)malloc(arg_count * sizeof(SSAValue*));
    for (int i = 0; i < arg_count; i++) {
        inst->operands[i] = args[i];
    }
    inst->operand_count = arg_count;
    inst->data.call_data.func_name = strdup(func_name);
    
    return inst;
}

SSAInstruction* create_ssa_return(SSAValue* value) {
    SSAInstruction* inst = (SSAInstruction*)malloc(sizeof(SSAInstruction));
    if (!inst) return NULL;
    
    inst->type = SSA_RETURN;
    inst->dest = NULL;
    if (value) {
        inst->operands = (SSAValue**)malloc(sizeof(SSAValue*));
        inst->operands[0] = value;
        inst->operand_count = 1;
        inst->data.return_data.value = value;
    } else {
        inst->operands = NULL;
        inst->operand_count = 0;
        inst->data.return_data.value = NULL;
    }
    
    return inst;
}

SSAInstruction* create_ssa_branch(SSAValue* condition, BasicBlock* true_target, BasicBlock* false_target) {
    SSAInstruction* inst = (SSAInstruction*)malloc(sizeof(SSAInstruction));
    if (!inst) return NULL;
    
    inst->type = SSA_BRANCH;
    inst->dest = NULL;
    inst->operands = (SSAValue**)malloc(sizeof(SSAValue*));
    inst->operands[0] = condition;
    inst->operand_count = 1;
    inst->data.branch_data.condition = condition;
    inst->data.branch_data.true_target = true_target;
    inst->data.branch_data.false_target = false_target;
    
    return inst;
}

SSAInstruction* create_ssa_jump(BasicBlock* target) {
    SSAInstruction* inst = (SSAInstruction*)malloc(sizeof(SSAInstruction));
    if (!inst) return NULL;
    
    inst->type = SSA_JUMP;
    inst->dest = NULL;
    inst->operands = NULL;
    inst->operand_count = 0;
    inst->data.jump_data.target = target;
    
    return inst;
}

void free_ssa_instruction(SSAInstruction* inst) {
    if (!inst) return;
    
    // Free operands array (but not the SSAValues themselves - they may be shared)
    if (inst->operands) {
        free(inst->operands);
    }
    
    // Free type-specific data
    if (inst->type == SSA_CALL && inst->data.call_data.func_name) {
        free(inst->data.call_data.func_name);
    } else if (inst->type == SSA_SWITCH && inst->data.switch_data.cases) {
        free(inst->data.switch_data.cases);
    }
    
    free(inst);
}

// --- Phi Node Functions ---

PhiNode* create_phi_node(SSAValue* dest) {
    PhiNode* phi = (PhiNode*)malloc(sizeof(PhiNode));
    if (!phi) return NULL;
    
    phi->dest = dest;
    phi->operands = NULL;
    phi->operand_count = 0;
    
    return phi;
}

void add_phi_operand(PhiNode* phi, BasicBlock* block, SSAValue* value) {
    if (!phi || !block || !value) return;
    
    // Resize operands array
    phi->operands = realloc(phi->operands, 
                           (phi->operand_count + 1) * sizeof(phi->operands[0]));
    if (!phi->operands) return;
    
    phi->operands[phi->operand_count].block = block;
    phi->operands[phi->operand_count].value = value;
    phi->operand_count++;
}

void free_phi_node(PhiNode* phi) {
    if (!phi) return;
    
    // Note: We don't free the SSAValues as they may be shared
    if (phi->operands) {
        free(phi->operands);
    }
    
    free(phi);
}

// --- Instruction List Functions ---

InstructionList* create_instruction_list() {
    InstructionList* list = (InstructionList*)malloc(sizeof(InstructionList));
    if (!list) return NULL;
    
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    
    return list;
}

void add_instruction(InstructionList* list, SSAInstruction* inst) {
    if (!list || !inst) return;
    
    if (list->count >= list->capacity) {
        int new_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        SSAInstruction** new_items = (SSAInstruction**)realloc(list->items,
                                                               new_capacity * sizeof(SSAInstruction*));
        if (!new_items) return;
        list->items = new_items;
        list->capacity = new_capacity;
    }
    
    list->items[list->count++] = inst;
}

void free_instruction_list(InstructionList* list) {
    if (!list) return;
    
    // Free all instructions
    for (int i = 0; i < list->count; i++) {
        free_ssa_instruction(list->items[i]);
    }
    
    free(list->items);
    free(list);
}

// --- Phi Node List Functions ---

PhiNodeList* create_phi_node_list() {
    PhiNodeList* list = (PhiNodeList*)malloc(sizeof(PhiNodeList));
    if (!list) return NULL;
    
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    
    return list;
}

void add_phi_node(PhiNodeList* list, PhiNode* phi) {
    if (!list || !phi) return;
    
    if (list->count >= list->capacity) {
        int new_capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        PhiNode** new_items = (PhiNode**)realloc(list->items,
                                                 new_capacity * sizeof(PhiNode*));
        if (!new_items) return;
        list->items = new_items;
        list->capacity = new_capacity;
    }
    
    list->items[list->count++] = phi;
}

void free_phi_node_list(PhiNodeList* list) {
    if (!list) return;
    
    // Free all phi nodes
    for (int i = 0; i < list->count; i++) {
        free_phi_node(list->items[i]);
    }
    
    free(list->items);
    free(list);
}

// Global counter for switch identifiers (matching hotstate approach)
static int next_switch_id = 0;

SSAInstruction* create_ssa_switch(SSAValue* expr, SwitchCase* cases, int case_count, BasicBlock* default_target) {
    SSAInstruction* inst = (SSAInstruction*)malloc(sizeof(SSAInstruction));
    if (!inst) return NULL;
    
    inst->type = SSA_SWITCH;
    inst->dest = NULL;
    inst->operands = (SSAValue**)malloc(sizeof(SSAValue*));
    inst->operands[0] = expr;
    inst->operand_count = 1;
    
    inst->data.switch_data.switch_expr = expr;
    inst->data.switch_data.switch_num = next_switch_id++;
    inst->data.switch_data.cases = cases;
    inst->data.switch_data.case_count = case_count;
    inst->data.switch_data.default_target = default_target;
    
    return inst;
}
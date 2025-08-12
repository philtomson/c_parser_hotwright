#define _GNU_SOURCE  // For strdup
#include "ssa_optimizer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// --- Context Management ---

OptimizationContext* create_optimization_context(CFG* cfg, HardwareContext* hw_ctx) {
    if (!cfg || !hw_ctx) {
        return NULL;
    }
    
    OptimizationContext* ctx = malloc(sizeof(OptimizationContext));
    if (!ctx) {
        return NULL;
    }
    
    ctx->cfg = cfg;
    ctx->hw_ctx = hw_ctx;
    
    // Enable key optimizations
    ctx->flags.constant_propagation = true;
    ctx->flags.copy_propagation = true;
    ctx->flags.dead_code_elimination = true;
    
    // Initialize statistics
    memset(&ctx->stats, 0, sizeof(OptimizationStats));
    
    // Count original instructions
    for (int i = 0; i < cfg->block_count; i++) {
        ctx->stats.original_instruction_count += cfg->blocks[i]->instructions->count;
    }
    
    // Initialize value tracking
    ctx->value_capacity = 256;  // Start with reasonable capacity
    ctx->value_count = 0;
    ctx->value_info = malloc(sizeof(ValueInfo) * ctx->value_capacity);
    if (!ctx->value_info) {
        free(ctx);
        return NULL;
    }
    
    return ctx;
}

void free_optimization_context(OptimizationContext* ctx) {
    if (!ctx) return;
    
    if (ctx->value_info) {
        free(ctx->value_info);
    }
    free(ctx);
}

// --- Main Optimization Entry Point ---

CFG* optimize_ssa_cfg(CFG* cfg, HardwareContext* hw_ctx) {
    if (!cfg || !hw_ctx) {
        return NULL;
    }
    
    printf("\n--- SSA Optimization Pass ---\n");
    
    OptimizationContext* ctx = create_optimization_context(cfg, hw_ctx);
    if (!ctx) {
        printf("Error: Failed to create optimization context\n");
        return cfg;
    }
    
    printf("Original instruction count: %d\n", ctx->stats.original_instruction_count);
    
    // Apply optimization passes in order
    bool changed = true;
    int pass = 1;
    
    while (changed && pass <= 5) {  // Limit to 5 passes to prevent infinite loops
        changed = false;
        printf("Optimization pass %d:\n", pass);
        
        // Pass 1: Constant propagation
        if (ctx->flags.constant_propagation) {
            if (constant_propagation_pass(ctx)) {
                changed = true;
                printf("  - Constants propagated: %d\n", ctx->stats.constants_propagated);
            }
        }
        
        // Pass 2: Copy propagation
        if (ctx->flags.copy_propagation) {
            if (copy_propagation_pass(ctx)) {
                changed = true;
                printf("  - Copies propagated: %d\n", ctx->stats.copies_propagated);
            }
        }
        
        // Pass 3: Dead code elimination
        if (ctx->flags.dead_code_elimination) {
            if (dead_code_elimination_pass(ctx)) {
                changed = true;
                printf("  - Dead instructions removed: %d\n", ctx->stats.dead_instructions_removed);
            }
        }
        
        pass++;
    }
    
    // Count final instructions
    for (int i = 0; i < cfg->block_count; i++) {
        ctx->stats.optimized_instruction_count += cfg->blocks[i]->instructions->count;
    }
    
    print_optimization_stats(&ctx->stats);
    
    free_optimization_context(ctx);
    return cfg;
}

// --- Value Analysis Functions ---

ValueInfo* get_value_info(OptimizationContext* ctx, SSAValue* value) {
    if (!ctx || !value) return NULL;
    
    // Look for existing value info
    for (int i = 0; i < ctx->value_count; i++) {
        if (ctx->value_info[i].value == value) {
            return &ctx->value_info[i];
        }
    }
    
    // Create new value info if not found
    if (ctx->value_count >= ctx->value_capacity) {
        // Expand capacity
        ctx->value_capacity *= 2;
        ctx->value_info = realloc(ctx->value_info, sizeof(ValueInfo) * ctx->value_capacity);
        if (!ctx->value_info) {
            return NULL;
        }
    }
    
    ValueInfo* info = &ctx->value_info[ctx->value_count++];
    memset(info, 0, sizeof(ValueInfo));
    info->value = value;
    return info;
}

void mark_value_as_constant(OptimizationContext* ctx, SSAValue* value, int constant) {
    ValueInfo* info = get_value_info(ctx, value);
    if (info) {
        info->is_constant = true;
        info->constant_value = constant;
        ctx->stats.constants_propagated++;
    }
}

void mark_value_as_copy(OptimizationContext* ctx, SSAValue* value, SSAValue* source) {
    ValueInfo* info = get_value_info(ctx, value);
    if (info) {
        info->is_copy = true;
        info->copy_source = source;
        ctx->stats.copies_propagated++;
    }
}

void mark_value_as_dead(OptimizationContext* ctx, SSAValue* value) {
    ValueInfo* info = get_value_info(ctx, value);
    if (info) {
        info->is_dead = true;
    }
}

// --- Analysis Functions ---

bool is_constant_instruction(SSAInstruction* instr) {
    if (!instr) return false;
    
    // Check for constant assignments like: x = 5
    if (instr->type == SSA_ASSIGN && instr->operand_count >= 1 && 
        instr->operands[0] && instr->operands[0]->type == SSA_CONST) {
        return true;
    }
    
    // Check for simple arithmetic with constants: x = 5 + 3
    if (instr->type == SSA_BINARY_OP && instr->operand_count >= 2 &&
        instr->operands[0] && instr->operands[1] &&
        instr->operands[0]->type == SSA_CONST && 
        instr->operands[1]->type == SSA_CONST) {
        return true;
    }
    
    return false;
}

bool is_copy_instruction(SSAInstruction* instr) {
    if (!instr) return false;
    
    // Check for simple copy: x = y
    if (instr->type == SSA_ASSIGN && instr->operand_count >= 1 && 
        instr->operands[0] && instr->operands[0]->type == SSA_VAR) {
        return true;
    }
    
    return false;
}

bool is_dead_instruction(SSAInstruction* instr, OptimizationContext* ctx) {
    if (!instr || !ctx) return false;
    
    // Never remove instructions that affect hardware state
    if (affects_hardware_state(instr, ctx->hw_ctx)) {
        return false;
    }
    
    // Check if the result is never used
    if (instr->dest) {
        return !is_value_used(instr->dest, ctx);
    }
    
    return false;
}

bool affects_hardware_state(SSAInstruction* instr, HardwareContext* hw_ctx) {
    if (!instr || !hw_ctx) return false;
    
    // Check if instruction assigns to a state variable
    if (instr->dest && instr->dest->type == SSA_VAR) {
        for (int i = 0; i < hw_ctx->state_count; i++) {
            if (strcmp(instr->dest->data.var.base_name, hw_ctx->states[i].name) == 0) {
                return true;
            }
        }
    }
    
    return false;
}

bool is_constant_value(SSAValue* value, OptimizationContext* ctx, int* constant_out) {
    if (!value || !ctx) return false;
    
    // Direct constant
    if (value->type == SSA_CONST) {
        if (constant_out) *constant_out = value->data.const_value;
        return true;
    }
    
    // Check if marked as constant through propagation
    ValueInfo* info = get_value_info(ctx, value);
    if (info && info->is_constant) {
        if (constant_out) *constant_out = info->constant_value;
        return true;
    }
    
    return false;
}

SSAValue* get_copy_source(SSAValue* value, OptimizationContext* ctx) {
    if (!value || !ctx) return NULL;
    
    ValueInfo* info = get_value_info(ctx, value);
    if (info && info->is_copy) {
        return info->copy_source;
    }
    
    return NULL;
}

bool is_value_used(SSAValue* value, OptimizationContext* ctx) {
    if (!value || !ctx) return false;
    
    // Scan all instructions to see if this value is used
    for (int i = 0; i < ctx->cfg->block_count; i++) {
        BasicBlock* block = ctx->cfg->blocks[i];
        for (int j = 0; j < block->instructions->count; j++) {
            SSAInstruction* instr = block->instructions->items[j];
            
            // Check operands
            for (int k = 0; k < instr->operand_count; k++) {
                if (instr->operands[k] == value) {
                    return true;
                }
            }
        }
    }
    
    return false;
}

// --- Optimization Passes ---

bool constant_propagation_pass(OptimizationContext* ctx) {
    if (!ctx) return false;
    
    bool changed = false;
    int initial_count = ctx->stats.constants_propagated;
    
    for (int i = 0; i < ctx->cfg->block_count; i++) {
        BasicBlock* block = ctx->cfg->blocks[i];
        
        for (int j = 0; j < block->instructions->count; j++) {
            SSAInstruction* instr = block->instructions->items[j];
            
            if (is_constant_instruction(instr)) {
                if (instr->type == SSA_ASSIGN && instr->dest) {
                    // Simple constant assignment: x = 5
                    mark_value_as_constant(ctx, instr->dest, instr->operands[0]->data.const_value);
                    changed = true;
                }
                else if (instr->type == SSA_BINARY_OP && instr->dest && instr->operand_count >= 2) {
                    // Constant folding: x = 5 + 3
                    int val1 = instr->operands[0]->data.const_value;
                    int val2 = instr->operands[1]->data.const_value;
                    int result = 0;
                    
                    switch (instr->data.op_data.op) {
                        case TOKEN_PLUS: result = val1 + val2; break;
                        case TOKEN_MINUS: result = val1 - val2; break;
                        case TOKEN_STAR: result = val1 * val2; break;
                        case TOKEN_SLASH: result = (val2 != 0) ? val1 / val2 : 0; break;
                        default: continue;
                    }
                    
                    mark_value_as_constant(ctx, instr->dest, result);
                    changed = true;
                }
            }
        }
    }
    
    return (ctx->stats.constants_propagated > initial_count);
}

bool copy_propagation_pass(OptimizationContext* ctx) {
    if (!ctx) return false;
    
    bool changed = false;
    int initial_count = ctx->stats.copies_propagated;
    
    for (int i = 0; i < ctx->cfg->block_count; i++) {
        BasicBlock* block = ctx->cfg->blocks[i];
        
        for (int j = 0; j < block->instructions->count; j++) {
            SSAInstruction* instr = block->instructions->items[j];
            
            if (is_copy_instruction(instr) && instr->dest) {
                // Simple copy: x = y
                mark_value_as_copy(ctx, instr->dest, instr->operands[0]);
                changed = true;
            }
        }
    }
    
    return (ctx->stats.copies_propagated > initial_count);
}

bool dead_code_elimination_pass(OptimizationContext* ctx) {
    if (!ctx) return false;
    
    bool changed = false;
    int initial_count = ctx->stats.dead_instructions_removed;
    
    for (int i = 0; i < ctx->cfg->block_count; i++) {
        BasicBlock* block = ctx->cfg->blocks[i];
        
        // Mark dead instructions (scan backwards to handle dependencies)
        for (int j = block->instructions->count - 1; j >= 0; j--) {
            SSAInstruction* instr = block->instructions->items[j];
            
            if (is_dead_instruction(instr, ctx)) {
                if (instr->dest) {
                    mark_value_as_dead(ctx, instr->dest);
                }
                ctx->stats.dead_instructions_removed++;
                changed = true;
            }
        }
    }
    
    return (ctx->stats.dead_instructions_removed > initial_count);
}

// --- Utility Functions ---

void print_optimization_stats(OptimizationStats* stats) {
    if (!stats) return;
    
    printf("\n=== SSA Optimization Results ===\n");
    printf("Original instructions: %d\n", stats->original_instruction_count);
    printf("Optimized instructions: %d\n", stats->optimized_instruction_count);
    printf("Reduction: %d instructions (%.1f%%)\n", 
           stats->original_instruction_count - stats->optimized_instruction_count,
           100.0 * (stats->original_instruction_count - stats->optimized_instruction_count) / 
           (float)stats->original_instruction_count);
    printf("Constants propagated: %d\n", stats->constants_propagated);
    printf("Copies propagated: %d\n", stats->copies_propagated);
    printf("Dead instructions removed: %d\n", stats->dead_instructions_removed);
    printf("\n");
}
#ifndef SSA_OPTIMIZER_H
#define SSA_OPTIMIZER_H

#include "cfg.h"
#include "hw_analyzer.h"
#include <stdbool.h>

// Optimization flags
typedef struct {
    bool constant_propagation;
    bool copy_propagation;
    bool dead_code_elimination;
} OptimizationFlags;

// Optimization statistics
typedef struct {
    int original_instruction_count;
    int optimized_instruction_count;
    int constants_propagated;
    int copies_propagated;
    int dead_instructions_removed;
} OptimizationStats;

// Value tracking for optimization
typedef struct {
    SSAValue* value;
    bool is_constant;
    int constant_value;
    bool is_copy;
    SSAValue* copy_source;
    bool is_dead;
} ValueInfo;

// Optimization context
typedef struct {
    CFG* cfg;
    HardwareContext* hw_ctx;
    OptimizationFlags flags;
    OptimizationStats stats;
    ValueInfo* value_info;
    int value_count;
    int value_capacity;
} OptimizationContext;

// --- Core Optimization Functions ---

// Main optimization entry point
CFG* optimize_ssa_cfg(CFG* cfg, HardwareContext* hw_ctx);

// Individual optimization passes
bool constant_propagation_pass(OptimizationContext* ctx);
bool copy_propagation_pass(OptimizationContext* ctx);
bool dead_code_elimination_pass(OptimizationContext* ctx);

// --- Context Management ---

// Initialize optimization context
OptimizationContext* create_optimization_context(CFG* cfg, HardwareContext* hw_ctx);
void free_optimization_context(OptimizationContext* ctx);

// --- Value Analysis Functions ---

// Value tracking
ValueInfo* get_value_info(OptimizationContext* ctx, SSAValue* value);
void mark_value_as_constant(OptimizationContext* ctx, SSAValue* value, int constant);
void mark_value_as_copy(OptimizationContext* ctx, SSAValue* value, SSAValue* source);
void mark_value_as_dead(OptimizationContext* ctx, SSAValue* value);

// --- Analysis Functions ---

// Check if instruction can be optimized
bool is_constant_instruction(SSAInstruction* instr);
bool is_copy_instruction(SSAInstruction* instr);
bool is_dead_instruction(SSAInstruction* instr, OptimizationContext* ctx);
bool affects_hardware_state(SSAInstruction* instr, HardwareContext* hw_ctx);

// Value analysis
bool is_constant_value(SSAValue* value, OptimizationContext* ctx, int* constant_out);
SSAValue* get_copy_source(SSAValue* value, OptimizationContext* ctx);
bool is_value_used(SSAValue* value, OptimizationContext* ctx);

// --- Transformation Functions ---

// Apply optimizations
void replace_value_with_constant(OptimizationContext* ctx, SSAValue* value, int constant);
void replace_value_with_copy(OptimizationContext* ctx, SSAValue* value, SSAValue* source);
void remove_dead_instruction(OptimizationContext* ctx, SSAInstruction* instr);

// --- Utility Functions ---

// Print optimization statistics
void print_optimization_stats(OptimizationStats* stats);

#endif // SSA_OPTIMIZER_H
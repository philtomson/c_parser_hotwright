#ifndef CFG_UTILS_H
#define CFG_UTILS_H

#include "cfg.h"
#include <stdio.h>

// Visualization functions
void cfg_to_dot(CFG* cfg, const char* filename);
void cfg_to_dot_file(CFG* cfg, FILE* file);
void print_cfg(CFG* cfg);
void print_basic_block(BasicBlock* block);
void print_ssa_instruction(SSAInstruction* inst);
void print_phi_node(PhiNode* phi);

// Analysis functions
void find_unreachable_blocks(CFG* cfg);
int count_reachable_blocks(CFG* cfg);
void mark_reachable_blocks(BasicBlock* block);

// Debugging functions
void verify_cfg(CFG* cfg);
void check_cfg_edges(CFG* cfg);
void check_terminators(CFG* cfg);

// Helper functions
const char* ssa_instruction_type_to_string(SSAInstructionType type);
const char* token_type_to_op_string(TokenType type);

#endif // CFG_UTILS_H
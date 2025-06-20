#include "cfg_utils.h"
#include <stdlib.h>
#include <string.h>

// --- Visualization Functions ---

void cfg_to_dot(CFG* cfg, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Error: Could not open file %s for writing\n", filename);
        return;
    }
    
    cfg_to_dot_file(cfg, file);
    fclose(file);
}

void cfg_to_dot_file(CFG* cfg, FILE* file) {
    if (!cfg || !file) return;
    
    fprintf(file, "digraph CFG {\n");
    fprintf(file, "  rankdir=TB;\n");
    fprintf(file, "  node [shape=box];\n");
    fprintf(file, "  \n");
    
    // Write all blocks
    for (int i = 0; i < cfg->block_count; i++) {
        BasicBlock* block = cfg->blocks[i];
        
        // Determine block style
        const char* style = "";
        if (block == cfg->entry) {
            style = ", style=filled, fillcolor=lightgreen";
        } else if (block == cfg->exit) {
            style = ", style=filled, fillcolor=lightcoral";
        }
        
        // Write block label with instructions
        fprintf(file, "  bb%d [label=\"Block %d", block->id, block->id);
        if (block->label) {
            fprintf(file, " (%s)", block->label);
        }
        fprintf(file, "\\n");
        
        // Add phi nodes
        for (int j = 0; j < block->phi_nodes->count; j++) {
            PhiNode* phi = block->phi_nodes->items[j];
            char* dest_str = ssa_value_to_string(phi->dest);
            fprintf(file, "%s = φ(", dest_str);
            free(dest_str);
            
            for (int k = 0; k < phi->operand_count; k++) {
                if (k > 0) fprintf(file, ", ");
                char* val_str = ssa_value_to_string(phi->operands[k].value);
                fprintf(file, "%s", val_str);
                free(val_str);
            }
            fprintf(file, ")\\n");
        }
        
        // Add instructions
        for (int j = 0; j < block->instructions->count; j++) {
            SSAInstruction* inst = block->instructions->items[j];
            
            // Format instruction
            switch (inst->type) {
                case SSA_ASSIGN: {
                    char* dest_str = ssa_value_to_string(inst->dest);
                    char* src_str = ssa_value_to_string(inst->operands[0]);
                    fprintf(file, "%s = %s\\n", dest_str, src_str);
                    free(dest_str);
                    free(src_str);
                    break;
                }
                case SSA_BINARY_OP: {
                    char* dest_str = ssa_value_to_string(inst->dest);
                    char* left_str = ssa_value_to_string(inst->operands[0]);
                    char* right_str = ssa_value_to_string(inst->operands[1]);
                    const char* op_str = token_type_to_op_string(inst->data.op_data.op);
                    fprintf(file, "%s = %s %s %s\\n", dest_str, left_str, op_str, right_str);
                    free(dest_str);
                    free(left_str);
                    free(right_str);
                    break;
                }
                case SSA_CALL: {
                    char* dest_str = ssa_value_to_string(inst->dest);
                    fprintf(file, "%s = call %s(", dest_str, inst->data.call_data.func_name);
                    free(dest_str);
                    
                    for (int k = 0; k < inst->operand_count; k++) {
                        if (k > 0) fprintf(file, ", ");
                        char* arg_str = ssa_value_to_string(inst->operands[k]);
                        fprintf(file, "%s", arg_str);
                        free(arg_str);
                    }
                    fprintf(file, ")\\n");
                    break;
                }
                case SSA_RETURN: {
                    fprintf(file, "return");
                    if (inst->operand_count > 0) {
                        char* val_str = ssa_value_to_string(inst->operands[0]);
                        fprintf(file, " %s", val_str);
                        free(val_str);
                    }
                    fprintf(file, "\\n");
                    break;
                }
                case SSA_BRANCH: {
                    char* cond_str = ssa_value_to_string(inst->operands[0]);
                    fprintf(file, "if %s goto bb%d else bb%d\\n", 
                            cond_str,
                            inst->data.branch_data.true_target->id,
                            inst->data.branch_data.false_target->id);
                    free(cond_str);
                    break;
                }
                case SSA_JUMP: {
                    fprintf(file, "goto bb%d\\n", inst->data.jump_data.target->id);
                    break;
                }
                default:
                    fprintf(file, "%s\\n", ssa_instruction_type_to_string(inst->type));
                    break;
            }
        }
        
        fprintf(file, "\"%s];\n", style);
    }
    
    fprintf(file, "  \n");
    
    // Write edges
    for (int i = 0; i < cfg->block_count; i++) {
        BasicBlock* block = cfg->blocks[i];
        for (int j = 0; j < block->successor_count; j++) {
            BasicBlock* succ = block->successors[j];
            
            // Determine edge label based on last instruction
            const char* label = "";
            if (block->instructions->count > 0) {
                SSAInstruction* last = block->instructions->items[block->instructions->count - 1];
                if (last->type == SSA_BRANCH) {
                    if (succ == last->data.branch_data.true_target) {
                        label = " [label=\"T\"]";
                    } else if (succ == last->data.branch_data.false_target) {
                        label = " [label=\"F\"]";
                    }
                }
            }
            
            fprintf(file, "  bb%d -> bb%d%s;\n", block->id, succ->id, label);
        }
    }
    
    fprintf(file, "}\n");
}

void print_cfg(CFG* cfg) {
    if (!cfg) return;
    
    printf("=== CFG for function: %s ===\n", cfg->function_name ? cfg->function_name : "<anonymous>");
    printf("Entry: Block %d\n", cfg->entry ? cfg->entry->id : -1);
    printf("Exit: Block %d\n", cfg->exit ? cfg->exit->id : -1);
    printf("Total blocks: %d\n\n", cfg->block_count);
    
    for (int i = 0; i < cfg->block_count; i++) {
        print_basic_block(cfg->blocks[i]);
        printf("\n");
    }
}

void print_basic_block(BasicBlock* block) {
    if (!block) return;
    
    printf("Block %d", block->id);
    if (block->label) {
        printf(" (%s)", block->label);
    }
    printf(":\n");
    
    // Print predecessors
    printf("  Predecessors: ");
    if (block->predecessor_count == 0) {
        printf("none");
    } else {
        for (int i = 0; i < block->predecessor_count; i++) {
            if (i > 0) printf(", ");
            printf("bb%d", block->predecessors[i]->id);
        }
    }
    printf("\n");
    
    // Print phi nodes
    if (block->phi_nodes->count > 0) {
        printf("  Phi nodes:\n");
        for (int i = 0; i < block->phi_nodes->count; i++) {
            printf("    ");
            print_phi_node(block->phi_nodes->items[i]);
            printf("\n");
        }
    }
    
    // Print instructions
    if (block->instructions->count > 0) {
        printf("  Instructions:\n");
        for (int i = 0; i < block->instructions->count; i++) {
            printf("    ");
            print_ssa_instruction(block->instructions->items[i]);
            printf("\n");
        }
    }
    
    // Print successors
    printf("  Successors: ");
    if (block->successor_count == 0) {
        printf("none");
    } else {
        for (int i = 0; i < block->successor_count; i++) {
            if (i > 0) printf(", ");
            printf("bb%d", block->successors[i]->id);
        }
    }
    printf("\n");
}

void print_ssa_instruction(SSAInstruction* inst) {
    if (!inst) return;
    
    switch (inst->type) {
        case SSA_ASSIGN: {
            char* dest_str = ssa_value_to_string(inst->dest);
            char* src_str = ssa_value_to_string(inst->operands[0]);
            printf("%s = %s", dest_str, src_str);
            free(dest_str);
            free(src_str);
            break;
        }
        case SSA_BINARY_OP: {
            char* dest_str = ssa_value_to_string(inst->dest);
            char* left_str = ssa_value_to_string(inst->operands[0]);
            char* right_str = ssa_value_to_string(inst->operands[1]);
            const char* op_str = token_type_to_op_string(inst->data.op_data.op);
            printf("%s = %s %s %s", dest_str, left_str, op_str, right_str);
            free(dest_str);
            free(left_str);
            free(right_str);
            break;
        }
        case SSA_UNARY_OP: {
            char* dest_str = ssa_value_to_string(inst->dest);
            char* operand_str = ssa_value_to_string(inst->operands[0]);
            const char* op_str = token_type_to_op_string(inst->data.op_data.op);
            printf("%s = %s%s", dest_str, op_str, operand_str);
            free(dest_str);
            free(operand_str);
            break;
        }
        case SSA_CALL: {
            char* dest_str = ssa_value_to_string(inst->dest);
            printf("%s = call %s(", dest_str, inst->data.call_data.func_name);
            free(dest_str);
            
            for (int i = 0; i < inst->operand_count; i++) {
                if (i > 0) printf(", ");
                char* arg_str = ssa_value_to_string(inst->operands[i]);
                printf("%s", arg_str);
                free(arg_str);
            }
            printf(")");
            break;
        }
        case SSA_RETURN: {
            printf("return");
            if (inst->operand_count > 0) {
                char* val_str = ssa_value_to_string(inst->operands[0]);
                printf(" %s", val_str);
                free(val_str);
            }
            break;
        }
        case SSA_BRANCH: {
            char* cond_str = ssa_value_to_string(inst->operands[0]);
            printf("if %s goto bb%d else bb%d", 
                   cond_str,
                   inst->data.branch_data.true_target->id,
                   inst->data.branch_data.false_target->id);
            free(cond_str);
            break;
        }
        case SSA_JUMP: {
            printf("goto bb%d", inst->data.jump_data.target->id);
            break;
        }
        case SSA_PHI: {
            printf("PHI");
            break;
        }
        default:
            printf("%s", ssa_instruction_type_to_string(inst->type));
            break;
    }
}

void print_phi_node(PhiNode* phi) {
    if (!phi) return;
    
    char* dest_str = ssa_value_to_string(phi->dest);
    printf("%s = φ(", dest_str);
    free(dest_str);
    
    for (int i = 0; i < phi->operand_count; i++) {
        if (i > 0) printf(", ");
        char* val_str = ssa_value_to_string(phi->operands[i].value);
        printf("%s:bb%d", val_str, phi->operands[i].block->id);
        free(val_str);
    }
    printf(")");
}

// --- Analysis Functions ---

void find_unreachable_blocks(CFG* cfg) {
    if (!cfg || !cfg->entry) return;
    
    // Mark all blocks as not visited
    for (int i = 0; i < cfg->block_count; i++) {
        cfg->blocks[i]->visited = false;
    }
    
    // Mark reachable blocks starting from entry
    mark_reachable_blocks(cfg->entry);
    
    // Report unreachable blocks
    printf("Unreachable blocks: ");
    int unreachable_count = 0;
    for (int i = 0; i < cfg->block_count; i++) {
        if (!cfg->blocks[i]->visited) {
            if (unreachable_count > 0) printf(", ");
            printf("bb%d", cfg->blocks[i]->id);
            unreachable_count++;
        }
    }
    if (unreachable_count == 0) {
        printf("none");
    }
    printf("\n");
}

int count_reachable_blocks(CFG* cfg) {
    if (!cfg || !cfg->entry) return 0;
    
    // Mark all blocks as not visited
    for (int i = 0; i < cfg->block_count; i++) {
        cfg->blocks[i]->visited = false;
    }
    
    // Mark reachable blocks starting from entry
    mark_reachable_blocks(cfg->entry);
    
    // Count reachable blocks
    int count = 0;
    for (int i = 0; i < cfg->block_count; i++) {
        if (cfg->blocks[i]->visited) {
            count++;
        }
    }
    
    return count;
}

void mark_reachable_blocks(BasicBlock* block) {
    if (!block || block->visited) return;
    
    block->visited = true;
    
    // Recursively mark successors
    for (int i = 0; i < block->successor_count; i++) {
        mark_reachable_blocks(block->successors[i]);
    }
}

// --- Debugging Functions ---

void verify_cfg(CFG* cfg) {
    if (!cfg) {
        printf("Error: NULL CFG\n");
        return;
    }
    
    printf("=== CFG Verification ===\n");
    
    // Check entry and exit
    if (!cfg->entry) {
        printf("Error: No entry block\n");
    }
    if (!cfg->exit) {
        printf("Error: No exit block\n");
    }
    
    // Check edges
    check_cfg_edges(cfg);
    
    // Check terminators
    check_terminators(cfg);
    
    // Check for unreachable blocks
    int reachable = count_reachable_blocks(cfg);
    if (reachable < cfg->block_count) {
        printf("Warning: %d unreachable blocks\n", cfg->block_count - reachable);
        find_unreachable_blocks(cfg);
    }
    
    printf("=== End Verification ===\n");
}

void check_cfg_edges(CFG* cfg) {
    for (int i = 0; i < cfg->block_count; i++) {
        BasicBlock* block = cfg->blocks[i];
        
        // Check that each successor has this block as a predecessor
        for (int j = 0; j < block->successor_count; j++) {
            BasicBlock* succ = block->successors[j];
            bool found = false;
            
            for (int k = 0; k < succ->predecessor_count; k++) {
                if (succ->predecessors[k] == block) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                printf("Error: Edge bb%d -> bb%d not reflected in predecessors\n",
                       block->id, succ->id);
            }
        }
        
        // Check that each predecessor has this block as a successor
        for (int j = 0; j < block->predecessor_count; j++) {
            BasicBlock* pred = block->predecessors[j];
            bool found = false;
            
            for (int k = 0; k < pred->successor_count; k++) {
                if (pred->successors[k] == block) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                printf("Error: Edge bb%d -> bb%d not reflected in successors\n",
                       pred->id, block->id);
            }
        }
    }
}

void check_terminators(CFG* cfg) {
    for (int i = 0; i < cfg->block_count; i++) {
        BasicBlock* block = cfg->blocks[i];
        
        if (block->instructions->count == 0) {
            if (block->successor_count > 0) {
                printf("Warning: Block bb%d has no instructions but has successors\n", block->id);
            }
            continue;
        }
        
        SSAInstruction* last = block->instructions->items[block->instructions->count - 1];
        
        // Check terminator consistency
        switch (last->type) {
            case SSA_JUMP:
                if (block->successor_count != 1) {
                    printf("Error: Block bb%d with jump has %d successors (expected 1)\n",
                           block->id, block->successor_count);
                }
                break;
            case SSA_BRANCH:
                if (block->successor_count != 2) {
                    printf("Error: Block bb%d with branch has %d successors (expected 2)\n",
                           block->id, block->successor_count);
                }
                break;
            case SSA_RETURN:
                if (block->successor_count != 1 || block->successors[0] != cfg->exit) {
                    printf("Error: Block bb%d with return doesn't lead to exit\n", block->id);
                }
                break;
            default:
                if (block->successor_count > 0) {
                    printf("Warning: Block bb%d ends with %s but has successors\n",
                           block->id, ssa_instruction_type_to_string(last->type));
                }
                break;
        }
    }
}

// --- Helper Functions ---

const char* ssa_instruction_type_to_string(SSAInstructionType type) {
    switch (type) {
        case SSA_PHI: return "PHI";
        case SSA_ASSIGN: return "ASSIGN";
        case SSA_BINARY_OP: return "BINARY_OP";
        case SSA_UNARY_OP: return "UNARY_OP";
        case SSA_LOAD: return "LOAD";
        case SSA_STORE: return "STORE";
        case SSA_CALL: return "CALL";
        case SSA_RETURN: return "RETURN";
        case SSA_BRANCH: return "BRANCH";
        case SSA_JUMP: return "JUMP";
        default: return "UNKNOWN";
    }
}

const char* token_type_to_op_string(TokenType type) {
    switch (type) {
        case TOKEN_PLUS: return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_STAR: return "*";
        case TOKEN_SLASH: return "/";
        case TOKEN_LESS: return "<";
        case TOKEN_GREATER: return ">";
        case TOKEN_LESS_EQUAL: return "<=";
        case TOKEN_GREATER_EQUAL: return ">=";
        case TOKEN_EQUAL: return "==";
        case TOKEN_NOT_EQUAL: return "!=";
        case TOKEN_ASSIGN: return "=";
        default: return "?";
    }
}
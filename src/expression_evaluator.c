#define _POSIX_C_SOURCE 200809L // For strdup
#include "expression_evaluator.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h> // For fprintf

// Function to evaluate a binary operation on single-bit values
int eval_op(int lhv, TokenType op, int rhv) {
    lhv = lhv & 1; // Ensure single bit
    rhv = rhv & 1; // Ensure single bit

    switch (op) {
        case TOKEN_AND:
        case TOKEN_LOGICAL_AND:
            return lhv && rhv;
        case TOKEN_OR:
        case TOKEN_LOGICAL_OR:
            return lhv || rhv;
        case TOKEN_EQUAL:
            return lhv == rhv;
        case TOKEN_NOT_EQUAL:
            return lhv != rhv;
        case TOKEN_LESS:
            return lhv < rhv;
        case TOKEN_GREATER:
            return lhv > rhv;
        case TOKEN_LESS_EQUAL:
            return lhv <= rhv;
        case TOKEN_GREATER_EQUAL:
            return lhv >= rhv;
        default:
            fprintf(stderr, "Error: Unsupported operator for eval_op: %d\n", op);
            return 0; // Default to 0 or error
    }
}

// Function to create a SimulatedExpression from an AST Node
SimulatedExpression* create_simulated_expression(Node* ast_expr_node, HardwareContext* hw_ctx) {
    if (!ast_expr_node) return NULL;

    SimulatedExpression* sim_expr = (SimulatedExpression*)malloc(sizeof(SimulatedExpression));
    if (!sim_expr) {
        fprintf(stderr, "Error: Failed to allocate SimulatedExpression.\n");
        exit(EXIT_FAILURE);
    }
    memset(sim_expr, 0, sizeof(SimulatedExpression)); // Initialize to zeros

    sim_expr->type = ast_expr_node->type;
    sim_expr->LUT = NULL; // Will be allocated and populated later
    sim_expr->LUT_size = 0;
    sim_expr->dependent_input_mask = 0;

    switch (ast_expr_node->type) {
        case NODE_IDENTIFIER: {
            IdentifierNode* id_node = (IdentifierNode*)ast_expr_node;
            sim_expr->var_name = strdup(id_node->name);
            // Check if it's an input variable and update dependent_input_mask
            int input_num = get_input_number_by_name(hw_ctx, id_node->name);
            if (input_num != -1) {
                sim_expr->dependent_input_mask |= (1 << input_num);
            }
            break;
        }
        case NODE_NUMBER_LITERAL: {
            NumberLiteralNode* num_node = (NumberLiteralNode*)ast_expr_node;
            sim_expr->const_value = atoi(num_node->value);
            break;
        }
        case NODE_BOOL_LITERAL: {
            BoolLiteralNode* bool_node = (BoolLiteralNode*)ast_expr_node;
            sim_expr->const_value = bool_node->value;
            break;
        }
        case NODE_BINARY_OP: {
            BinaryOpNode* bin_op_node = (BinaryOpNode*)ast_expr_node;
            sim_expr->op_type = bin_op_node->op; // Store TokenType directly
            sim_expr->lhs = create_simulated_expression(bin_op_node->left, hw_ctx);
            sim_expr->rhs = create_simulated_expression(bin_op_node->right, hw_ctx);
            // Combine dependent_input_masks from children
            if (sim_expr->lhs) sim_expr->dependent_input_mask |= sim_expr->lhs->dependent_input_mask;
            if (sim_expr->rhs) sim_expr->dependent_input_mask |= sim_expr->rhs->dependent_input_mask;
            break;
        }
        case NODE_UNARY_OP: {
            UnaryOpNode* un_op_node = (UnaryOpNode*)ast_expr_node;
            sim_expr->op_type = un_op_node->op; // Store TokenType directly
            sim_expr->lhs = create_simulated_expression(un_op_node->operand, hw_ctx); // Unary ops use lhs for operand
            if (sim_expr->lhs) sim_expr->dependent_input_mask |= sim_expr->lhs->dependent_input_mask;
            break;
        }
        default:
            fprintf(stderr, "Warning: Unsupported AST node type for simulated expression: %d\n", ast_expr_node->type);
            break;
    }
    return sim_expr;
}

// Function to evaluate a simulated expression and populate its LUT
void eval_simulated_expression(SimulatedExpression* sim_expr, HardwareContext* hw_ctx, int num_total_input_vars) {
    if (!sim_expr) return;

    // Calculate LUT_size based on total number of input variables
    sim_expr->LUT_size = 1 << num_total_input_vars; // 2^num_total_input_vars
    sim_expr->LUT = (uint8_t*)malloc(sizeof(uint8_t) * sim_expr->LUT_size);
    if (!sim_expr->LUT) {
        fprintf(stderr, "Error: Failed to allocate LUT for simulated expression.\n");
        exit(EXIT_FAILURE);
    }
    
    // Iterate through all possible input combinations
    for (int i = 0; i < sim_expr->LUT_size; i++) {
        switch (sim_expr->type) {
            case NODE_IDENTIFIER: {
                // Determine the value of the identifier for the current input combination (i)
                // This is the "eigenLUT" generation part
                int input_num = get_input_number_by_name(hw_ctx, sim_expr->var_name);
                if (input_num != -1) {
                    sim_expr->LUT[i] = (i >> input_num) & 1; // Extract the bit corresponding to this input variable
                } else {
                    // Not an input variable, treat as a constant 0 for evaluation if not found
                    // Or, if it's a state variable, its value would depend on the current state.
                    // For now, assume 0 for non-input identifiers in expressions.
                    sim_expr->LUT[i] = 0; 
                }
                break;
            }
            case NODE_NUMBER_LITERAL:
            case NODE_BOOL_LITERAL: {
                sim_expr->LUT[i] = sim_expr->const_value & 1;
                break;
            }
            case NODE_BINARY_OP: {
                // Recursively evaluate LHS and RHS
                eval_simulated_expression(sim_expr->lhs, hw_ctx, num_total_input_vars);
                eval_simulated_expression(sim_expr->rhs, hw_ctx, num_total_input_vars);
                // Combine LUTs using eval_op
                sim_expr->LUT[i] = eval_op(sim_expr->lhs->LUT[i], sim_expr->op_type, sim_expr->rhs->LUT[i]);
                break;
            }
            case NODE_UNARY_OP: {
                // Recursively evaluate operand
                eval_simulated_expression(sim_expr->lhs, hw_ctx, num_total_input_vars); // Unary ops use lhs for operand
                // Apply unary op
                if (sim_expr->op_type == TOKEN_NOT) { // Logical NOT
                    sim_expr->LUT[i] = !sim_expr->lhs->LUT[i];
                } else {
                    fprintf(stderr, "Warning: Unsupported unary operator for eval_simulated_expression: %d\n", sim_expr->op_type);
                    sim_expr->LUT[i] = 0;
                }
                break;
            }
            default:
                fprintf(stderr, "Warning: Unsupported AST node type for eval_simulated_expression: %d\n", sim_expr->type);
                sim_expr->LUT[i] = 0;
                break;
        }
    }
}

// Function to free a simulated expression tree
void free_simulated_expression(SimulatedExpression* sim_expr) {
    if (!sim_expr) return;

    if (sim_expr->lhs) {
        free_simulated_expression(sim_expr->lhs);
    }
    if (sim_expr->rhs) {
        free_simulated_expression(sim_expr->rhs);
    }
    if (sim_expr->var_name) {
        free(sim_expr->var_name);
    }
    if (sim_expr->LUT) {
        free(sim_expr->LUT);
    }
    free(sim_expr);
}
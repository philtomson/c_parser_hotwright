#ifndef EXPRESSION_EVALUATOR_H
#define EXPRESSION_EVALUATOR_H

#include "ast.h"    // For NodeType
#include "lexer.h"  // For TokenType
#include "hw_analyzer.h" // For HardwareContext

// Structure to represent a simulated expression for building the Uber LUT
typedef struct SimulatedExpression {
    NodeType type;      // Type of the expression node (e.g., NODE_BINARY_OP, NODE_IDENTIFIER)
    TokenType op_type;  // Operator type for binary/unary ops (e.g., TOKEN_AND, TOKEN_NOT)
    struct SimulatedExpression* lhs; // Left-hand side operand for binary ops
    struct SimulatedExpression* rhs; // Right-hand side operand for binary ops
    char* var_name;     // Variable name for identifiers
    int const_value;    // Value for number/bool literals

    uint8_t* LUT;       // Truth table (Uber LUT fragment) for this expression
    int LUT_size;       // Size of the LUT (2^num_dependent_inputs)
    
    // For identifying which input variables this expression depends on
    // This could be a bitmask or a list of input_numbers
    uint32_t dependent_input_mask; // Bitmask of dependent input variable numbers
} SimulatedExpression;

// Function prototypes for the Expression Evaluator/Simulator
SimulatedExpression* create_simulated_expression(Node* ast_expr_node, HardwareContext* hw_ctx);
void eval_simulated_expression(SimulatedExpression* sim_expr, HardwareContext* hw_ctx, int num_total_input_vars);
int eval_op(int lhv, TokenType op, int rhv);
void free_simulated_expression(SimulatedExpression* sim_expr);

#endif // EXPRESSION_EVALUATOR_H
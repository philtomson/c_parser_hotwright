#ifndef CFG_BUILDER_H
#define CFG_BUILDER_H

#include "cfg.h"
#include "ast.h"

// Builder context to track state during CFG construction
typedef struct CFGBuilderContext {
    CFG* cfg;
    BasicBlock* current_block;
    
    // For tracking loop contexts (for break/continue)
    struct LoopContext {
        BasicBlock* header;
        BasicBlock* exit;
        struct LoopContext* parent;
    }* loop_context;
    
    // For tracking variable versions (simple version for now)
    struct VarVersion {
        char* name;
        int version;
    }* var_versions;
    int var_count;
    int var_capacity;
    
    // Temporary counter
    int next_temp_id;
} CFGBuilderContext;

// Main entry points
CFG* build_cfg_from_ast(Node* ast);
CFG* build_function_cfg(FunctionDefNode* func);

// Context management
CFGBuilderContext* create_builder_context(CFG* cfg);
void free_builder_context(CFGBuilderContext* ctx);

// Variable version tracking (simplified for initial implementation)
int get_var_version(CFGBuilderContext* ctx, const char* name);
int increment_var_version(CFGBuilderContext* ctx, const char* name);
SSAValue* get_current_var_value(CFGBuilderContext* ctx, const char* name);

// Statement processing
BasicBlock* process_statement(CFGBuilderContext* ctx, Node* stmt, BasicBlock* current);
BasicBlock* process_block(CFGBuilderContext* ctx, BlockNode* block, BasicBlock* current);
BasicBlock* process_var_decl(CFGBuilderContext* ctx, VarDeclNode* decl, BasicBlock* current);
BasicBlock* process_expression_statement(CFGBuilderContext* ctx, ExpressionStatementNode* expr_stmt, BasicBlock* current);
BasicBlock* process_if_statement(CFGBuilderContext* ctx, IfNode* if_stmt, BasicBlock* current);
BasicBlock* process_while_loop(CFGBuilderContext* ctx, WhileNode* while_stmt, BasicBlock* current);
BasicBlock* process_for_loop(CFGBuilderContext* ctx, ForNode* for_stmt, BasicBlock* current);
BasicBlock* process_return_statement(CFGBuilderContext* ctx, ReturnNode* ret_stmt, BasicBlock* current);
BasicBlock* process_break_statement(CFGBuilderContext* ctx, BreakNode* break_stmt, BasicBlock* current);

// Expression processing (returns SSA value)
SSAValue* process_expression(CFGBuilderContext* ctx, Node* expr, BasicBlock* current);
SSAValue* process_binary_op(CFGBuilderContext* ctx, BinaryOpNode* bin_op, BasicBlock* current);
SSAValue* process_assignment(CFGBuilderContext* ctx, AssignmentNode* assign, BasicBlock* current);
SSAValue* process_identifier(CFGBuilderContext* ctx, IdentifierNode* ident, BasicBlock* current);
SSAValue* process_number_literal(CFGBuilderContext* ctx, NumberLiteralNode* num, BasicBlock* current);
SSAValue* process_function_call(CFGBuilderContext* ctx, FunctionCallNode* call, BasicBlock* current);

// Helper functions
void finalize_block(BasicBlock* block);
BasicBlock* split_block(CFGBuilderContext* ctx, BasicBlock* block, const char* label);

#endif // CFG_BUILDER_H
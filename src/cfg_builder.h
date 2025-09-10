#ifndef CFG_BUILDER_H
#define CFG_BUILDER_H

#include "cfg.h"
#include "ast.h"


// An entry mapping a lable name to it's BasicBlock
typedef struct {
    char* label_name;
    BasicBlock* block;
} LabelMapEntry;

typedef struct {
    LabelMapEntry* entries;
    int count;
    int capacity;
} LabelMap;

// A node in a linked list of BasicBlocks that are waiting for a label
typedef struct PendingGotoNode {
    BasicBlock* waiting_block;
    SSAInstruction* jump_instr;  // The jump instruction that needs target resolution
    struct PendingGotoNode* next;
} PendingGotoNode;

// An entry for a label that hasn't been seen yet
typedef struct {
    char* label_name;
    PendingGotoNode* pending_list_head;
} UnresolvedGotoEntry;

// The "map" for unresolved gotos
typedef struct {
    UnresolvedGotoEntry* entries;
    int count;
    int capacity;
} UnresolvedGotoMap;


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
    
    // For tracking switch contexts (for break statements)
    struct SwitchContext {
        BasicBlock* exit_block;
        struct SwitchContext* parent;
    }* switch_context;
    
    // For tracking variable versions with proper scoping
    struct VarVersion {
        char* name;
        int version;
        int scope_level;  // Track which scope this variable belongs to
    }* var_versions;
    int var_count;
    int var_capacity;
    
    // Scope tracking
    int current_scope_level;
    
    // Temporary counter
    int next_temp_id;
    
    // Goto/Label support
    LabelMap* label_map;                    // Maps resolved labels to blocks
    UnresolvedGotoMap* unresolved_gotos;    // Tracks pending gotos
} CFGBuilderContext;

// Main entry points
CFG* build_cfg_from_ast(Node* ast);
CFG* build_function_cfg(FunctionDefNode* func);

// Context management
CFGBuilderContext* create_builder_context(CFG* cfg);
void free_builder_context(CFGBuilderContext* ctx);

// Variable version tracking with scoping support
int get_var_version(CFGBuilderContext* ctx, const char* name);
void enter_scope(CFGBuilderContext* ctx);
void exit_scope(CFGBuilderContext* ctx);
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
BasicBlock* process_switch_statement(CFGBuilderContext* ctx, SwitchNode* switch_stmt, BasicBlock* current);
BasicBlock* process_return_statement(CFGBuilderContext* ctx, ReturnNode* ret_stmt, BasicBlock* current);
BasicBlock* process_break_statement(CFGBuilderContext* ctx, BreakNode* break_stmt, BasicBlock* current);
BasicBlock* process_continue_statement(CFGBuilderContext* ctx, ContinueNode* continue_stmt, BasicBlock* current);

// Goto/Label processing functions
BasicBlock* process_goto_statement(CFGBuilderContext* ctx, GotoNode* goto_node, BasicBlock* current);
BasicBlock* process_label_statement(CFGBuilderContext* ctx, LabelNode* label_node, BasicBlock* current);
void register_label(CFGBuilderContext* ctx, const char* label_name, BasicBlock* block);
void resolve_pending_gotos(CFGBuilderContext* ctx, const char* label_name, BasicBlock* target_block);
int validate_goto_for_hardware(CFGBuilderContext* ctx, const char* label_name);

// Label map management functions
LabelMap* create_label_map(void);
void free_label_map(LabelMap* map);
BasicBlock* lookup_label(LabelMap* map, const char* label_name);
void add_label(LabelMap* map, const char* label_name, BasicBlock* block);

// Unresolved goto map management functions
UnresolvedGotoMap* create_unresolved_goto_map(void);
void free_unresolved_goto_map(UnresolvedGotoMap* map);
void add_unresolved_goto(UnresolvedGotoMap* map, const char* label_name, BasicBlock* waiting_block, SSAInstruction* jump_instr);
PendingGotoNode* get_pending_gotos(UnresolvedGotoMap* map, const char* label_name);
void remove_resolved_gotos(UnresolvedGotoMap* map, const char* label_name);

// Expression processing (returns SSA value)
SSAValue* process_expression(CFGBuilderContext* ctx, Node* expr, BasicBlock* current);
SSAValue* process_binary_op(CFGBuilderContext* ctx, BinaryOpNode* bin_op, BasicBlock* current);
SSAValue* process_unary_op(CFGBuilderContext* ctx, UnaryOpNode* unary_op, BasicBlock* current);
SSAValue* process_assignment(CFGBuilderContext* ctx, AssignmentNode* assign, BasicBlock* current);
SSAValue* process_identifier(CFGBuilderContext* ctx, IdentifierNode* ident, BasicBlock* current);
SSAValue* process_number_literal(CFGBuilderContext* ctx, NumberLiteralNode* num, BasicBlock* current);
SSAValue* process_function_call(CFGBuilderContext* ctx, FunctionCallNode* call, BasicBlock* current);
SSAValue* process_array_access(CFGBuilderContext* ctx, ArrayAccessNode* arr_access, BasicBlock* current);
SSAValue* process_bool_literal(CFGBuilderContext* ctx, BoolLiteralNode* bool_lit, BasicBlock* current);
SSAValue* process_initializer_list(CFGBuilderContext* ctx, InitializerListNode* init_list, BasicBlock* current);

// Helper functions
void finalize_block(BasicBlock* block);
BasicBlock* split_block(CFGBuilderContext* ctx, BasicBlock* block, const char* label);

#endif // CFG_BUILDER_H
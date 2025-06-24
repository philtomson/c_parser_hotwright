#define _GNU_SOURCE  // For strdup
#include "cfg_builder.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// --- Context Management ---

CFGBuilderContext* create_builder_context(CFG* cfg) {
    CFGBuilderContext* ctx = (CFGBuilderContext*)malloc(sizeof(CFGBuilderContext));
    if (!ctx) return NULL;
    
    ctx->cfg = cfg;
    ctx->current_block = NULL;
    ctx->loop_context = NULL;
    ctx->switch_context = NULL;
    ctx->var_versions = NULL;
    ctx->var_count = 0;
    ctx->var_capacity = 0;
    ctx->next_temp_id = 0;
    ctx->current_scope_level = 0;  // Start at global scope
    
    return ctx;
}

void free_builder_context(CFGBuilderContext* ctx) {
    if (!ctx) return;
    
    // Free variable version tracking
    for (int i = 0; i < ctx->var_count; i++) {
        free(ctx->var_versions[i].name);
    }
    free(ctx->var_versions);
    
    // Free loop context stack
    while (ctx->loop_context) {
        struct LoopContext* next = ctx->loop_context->parent;
        free(ctx->loop_context);
        ctx->loop_context = next;
    }
    
    // Free switch context stack
    while (ctx->switch_context) {
        struct SwitchContext* next = ctx->switch_context->parent;
        free(ctx->switch_context);
        ctx->switch_context = next;
    }
    
    free(ctx);
}

// --- Variable Version Tracking ---

int get_var_version(CFGBuilderContext* ctx, const char* name) {
    // Search from most recent (highest scope) to oldest (lowest scope)
    // This implements proper variable shadowing
    for (int i = ctx->var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->var_versions[i].name, name) == 0) {
            return ctx->var_versions[i].version;
        }
    }
    return 0; // Variable not found, return version 0
}

int increment_var_version(CFGBuilderContext* ctx, const char* name) {
    // For variable declarations, always create a new version in current scope
    // This handles both new variables and variable shadowing
    
    // Add new variable version
    if (ctx->var_count >= ctx->var_capacity) {
        int new_capacity = ctx->var_capacity == 0 ? 8 : ctx->var_capacity * 2;
        struct VarVersion* new_versions = (struct VarVersion*)realloc(ctx->var_versions,
                                                                      new_capacity * sizeof(struct VarVersion));
        if (!new_versions) return 0;
        ctx->var_versions = new_versions;
        ctx->var_capacity = new_capacity;
    }
    
    // Find the highest version number for this variable name across all scopes
    int max_version = 0;
    for (int i = 0; i < ctx->var_count; i++) {
        if (strcmp(ctx->var_versions[i].name, name) == 0) {
            if (ctx->var_versions[i].version > max_version) {
                max_version = ctx->var_versions[i].version;
            }
        }
    }
    
    ctx->var_versions[ctx->var_count].name = strdup(name);
    ctx->var_versions[ctx->var_count].version = max_version + 1;
    ctx->var_versions[ctx->var_count].scope_level = ctx->current_scope_level;
    ctx->var_count++;
    
    return max_version + 1;
}

// --- Scope Management ---

void enter_scope(CFGBuilderContext* ctx) {
    ctx->current_scope_level++;
}

void exit_scope(CFGBuilderContext* ctx) {
    if (ctx->current_scope_level <= 0) return; // Don't go below global scope
    
    // Remove all variables from the current scope level
    int write_index = 0;
    for (int read_index = 0; read_index < ctx->var_count; read_index++) {
        if (ctx->var_versions[read_index].scope_level < ctx->current_scope_level) {
            // Keep variables from outer scopes
            if (write_index != read_index) {
                ctx->var_versions[write_index] = ctx->var_versions[read_index];
            }
            write_index++;
        } else {
            // Free variables from current scope
            free(ctx->var_versions[read_index].name);
        }
    }
    ctx->var_count = write_index;
    ctx->current_scope_level--;
}

SSAValue* get_current_var_value(CFGBuilderContext* ctx, const char* name) {
    int version = get_var_version(ctx, name);
    return create_ssa_var(name, version);
}

// --- Main Entry Points ---

CFG* build_cfg_from_ast(Node* ast) {
    if (!ast || ast->type != NODE_PROGRAM) return NULL;
    
    ProgramNode* program = (ProgramNode*)ast;
    if (program->functions->count == 0) return NULL;
    
    // Find the first function (skip global variable declarations)
    Node* first_func = NULL;
    for (int i = 0; i < program->functions->count; i++) {
        Node* item = program->functions->items[i];
        if (item->type == NODE_FUNCTION_DEF) {
            first_func = item;
            break;
        }
    }
    
    if (!first_func) return NULL;
    
    return build_function_cfg((FunctionDefNode*)first_func);
}

CFG* build_function_cfg(FunctionDefNode* func) {
    CFG* cfg = create_cfg(func->name);
    if (!cfg) return NULL;
    
    CFGBuilderContext* ctx = create_builder_context(cfg);
    if (!ctx) {
        free_cfg(cfg);
        return NULL;
    }
    
    // Create entry and exit blocks
    cfg->entry = create_basic_block(cfg, "entry");
    cfg->exit = create_basic_block(cfg, "exit");
    ctx->current_block = cfg->entry;
    
    // Enter function scope (scope level 1)
    enter_scope(ctx);
    
    // Process function parameters (create initial versions)
    for (int i = 0; i < func->parameters->count; i++) {
        IdentifierNode* param = (IdentifierNode*)func->parameters->items[i];
        int version = increment_var_version(ctx, param->name);
        SSAValue* param_value = create_ssa_var(param->name, version);
        // In a full implementation, we'd add parameter initialization instructions
    }
    
    // Process function body
    BasicBlock* last_block = process_statement(ctx, func->body, ctx->current_block);
    
    // Exit function scope
    exit_scope(ctx);
    
    // If the last block doesn't end with a return, add edge to exit
    if (last_block && last_block->instructions->count > 0) {
        SSAInstruction* last_inst = last_block->instructions->items[last_block->instructions->count - 1];
        if (last_inst->type != SSA_RETURN) {
            add_edge(last_block, cfg->exit);
            // Add implicit return
            add_instruction(last_block->instructions, create_ssa_return(NULL));
        }
    } else if (last_block) {
        add_edge(last_block, cfg->exit);
        add_instruction(last_block->instructions, create_ssa_return(NULL));
    }
    
    free_builder_context(ctx);
    return cfg;
}

// --- Statement Processing ---

BasicBlock* process_statement(CFGBuilderContext* ctx, Node* stmt, BasicBlock* current) {
    if (!stmt || !current) return current;
    
    switch (stmt->type) {
        case NODE_BLOCK:
            return process_block(ctx, (BlockNode*)stmt, current);
        case NODE_VAR_DECL:
            return process_var_decl(ctx, (VarDeclNode*)stmt, current);
        case NODE_EXPRESSION_STATEMENT:
            return process_expression_statement(ctx, (ExpressionStatementNode*)stmt, current);
        case NODE_IF:
            return process_if_statement(ctx, (IfNode*)stmt, current);
        case NODE_WHILE:
            return process_while_loop(ctx, (WhileNode*)stmt, current);
        case NODE_FOR:
            return process_for_loop(ctx, (ForNode*)stmt, current);
        case NODE_SWITCH:
            return process_switch_statement(ctx, (SwitchNode*)stmt, current);
        case NODE_RETURN:
            return process_return_statement(ctx, (ReturnNode*)stmt, current);
        case NODE_BREAK:
            return process_break_statement(ctx, (BreakNode*)stmt, current);
        case NODE_CONTINUE:
            return process_continue_statement(ctx, (ContinueNode*)stmt, current);
        default:
            // Unsupported statement type
            return current;
    }
}

BasicBlock* process_block(CFGBuilderContext* ctx, BlockNode* block, BasicBlock* current) {
    // Enter new scope for this block
    enter_scope(ctx);
    
    for (int i = 0; i < block->statements->count; i++) {
        current = process_statement(ctx, block->statements->items[i], current);
        if (!current) break; // Block terminated early (e.g., by return)
    }
    
    // Exit scope when leaving the block
    exit_scope(ctx);
    return current;
}

BasicBlock* process_var_decl(CFGBuilderContext* ctx, VarDeclNode* decl, BasicBlock* current) {
    int version = increment_var_version(ctx, decl->var_name);
    SSAValue* dest = create_ssa_var(decl->var_name, version);
    
    if (decl->initializer) {
        SSAValue* init_value = process_expression(ctx, decl->initializer, current);
        SSAInstruction* assign = create_ssa_assign(dest, init_value);
        add_instruction(current->instructions, assign);
    } else {
        // Initialize to 0
        SSAValue* zero = create_ssa_const(0);
        SSAInstruction* assign = create_ssa_assign(dest, zero);
        add_instruction(current->instructions, assign);
        free_ssa_value(zero);
    }
    
    return current;
}

BasicBlock* process_expression_statement(CFGBuilderContext* ctx, ExpressionStatementNode* expr_stmt, BasicBlock* current) {
    process_expression(ctx, expr_stmt->expression, current);
    return current;
}

BasicBlock* process_if_statement(CFGBuilderContext* ctx, IfNode* if_stmt, BasicBlock* current) {
    // Evaluate condition
    SSAValue* condition = process_expression(ctx, if_stmt->condition, current);
    
    // Create blocks
    BasicBlock* then_block = create_basic_block(ctx->cfg, "if.then");
    BasicBlock* else_block = if_stmt->else_branch ? create_basic_block(ctx->cfg, "if.else") : NULL;
    BasicBlock* merge_block = create_basic_block(ctx->cfg, "if.merge");
    
    // Add branch instruction
    SSAInstruction* branch = create_ssa_branch(condition, then_block, 
                                              else_block ? else_block : merge_block);
    add_instruction(current->instructions, branch);
    
    // Add edges
    add_edge(current, then_block);
    add_edge(current, else_block ? else_block : merge_block);
    
    // Process then branch
    BasicBlock* then_exit = process_statement(ctx, if_stmt->then_branch, then_block);
    if (then_exit) {
        add_edge(then_exit, merge_block);
        add_instruction(then_exit->instructions, create_ssa_jump(merge_block));
    }
    
    // Process else branch if exists
    if (else_block) {
        BasicBlock* else_exit = process_statement(ctx, if_stmt->else_branch, else_block);
        if (else_exit) {
            add_edge(else_exit, merge_block);
            add_instruction(else_exit->instructions, create_ssa_jump(merge_block));
        }
    }
    
    return merge_block;
}

BasicBlock* process_while_loop(CFGBuilderContext* ctx, WhileNode* while_stmt, BasicBlock* current) {
    // Create blocks
    BasicBlock* header = create_basic_block(ctx->cfg, "while.header");
    BasicBlock* body = create_basic_block(ctx->cfg, "while.body");
    BasicBlock* exit = create_basic_block(ctx->cfg, "while.exit");
    
    // Jump to header
    add_instruction(current->instructions, create_ssa_jump(header));
    add_edge(current, header);
    
    // Evaluate condition in header
    SSAValue* condition = process_expression(ctx, while_stmt->condition, header);
    SSAInstruction* branch = create_ssa_branch(condition, body, exit);
    add_instruction(header->instructions, branch);
    add_edge(header, body);
    add_edge(header, exit);
    
    // Push loop context
    struct LoopContext* loop_ctx = (struct LoopContext*)malloc(sizeof(struct LoopContext));
    loop_ctx->header = header;
    loop_ctx->exit = exit;
    loop_ctx->parent = ctx->loop_context;
    ctx->loop_context = loop_ctx;
    
    // Process body
    BasicBlock* body_exit = process_statement(ctx, while_stmt->body, body);
    if (body_exit) {
        add_edge(body_exit, header);
        add_instruction(body_exit->instructions, create_ssa_jump(header));
    }
    
    // Pop loop context
    ctx->loop_context = loop_ctx->parent;
    free(loop_ctx);
    
    return exit;
}

BasicBlock* process_for_loop(CFGBuilderContext* ctx, ForNode* for_stmt, BasicBlock* current) {
    // Process initialization
    if (for_stmt->init) {
        process_expression(ctx, for_stmt->init, current);
    }
    
    // Create blocks
    BasicBlock* header = create_basic_block(ctx->cfg, "for.header");
    BasicBlock* body = create_basic_block(ctx->cfg, "for.body");
    BasicBlock* update = create_basic_block(ctx->cfg, "for.update");
    BasicBlock* exit = create_basic_block(ctx->cfg, "for.exit");
    
    // Jump to header
    add_instruction(current->instructions, create_ssa_jump(header));
    add_edge(current, header);
    
    // Evaluate condition in header (if exists)
    if (for_stmt->condition) {
        SSAValue* condition = process_expression(ctx, for_stmt->condition, header);
        SSAInstruction* branch = create_ssa_branch(condition, body, exit);
        add_instruction(header->instructions, branch);
        add_edge(header, body);
        add_edge(header, exit);
    } else {
        // No condition means infinite loop
        add_instruction(header->instructions, create_ssa_jump(body));
        add_edge(header, body);
    }
    
    // Push loop context
    struct LoopContext* loop_ctx = (struct LoopContext*)malloc(sizeof(struct LoopContext));
    loop_ctx->header = header;
    loop_ctx->exit = exit;
    loop_ctx->parent = ctx->loop_context;
    ctx->loop_context = loop_ctx;
    
    // Process body
    BasicBlock* body_exit = process_statement(ctx, for_stmt->body, body);
    if (body_exit) {
        add_edge(body_exit, update);
        add_instruction(body_exit->instructions, create_ssa_jump(update));
    }
    
    // Process update
    if (for_stmt->update) {
        process_expression(ctx, for_stmt->update, update);
    }
    add_edge(update, header);
    add_instruction(update->instructions, create_ssa_jump(header));
    
    // Pop loop context
    ctx->loop_context = loop_ctx->parent;
    free(loop_ctx);
    
    return exit;
}

BasicBlock* process_return_statement(CFGBuilderContext* ctx, ReturnNode* ret_stmt, BasicBlock* current) {
    SSAValue* return_value = NULL;
    if (ret_stmt->return_value) {
        return_value = process_expression(ctx, ret_stmt->return_value, current);
    }
    
    SSAInstruction* ret_inst = create_ssa_return(return_value);
    add_instruction(current->instructions, ret_inst);
    
    // Add edge to exit block if it exists
    if (ctx->cfg->exit) {
        add_edge(current, ctx->cfg->exit);
    }
    
    return NULL; // No successor block after return
}

BasicBlock* process_break_statement(CFGBuilderContext* ctx, BreakNode* break_stmt, BasicBlock* current) {
    // Check if we're in a switch context first (inner-most context)
    if (ctx->switch_context) {
        // Jump to switch exit
        add_instruction(current->instructions, create_ssa_jump(ctx->switch_context->exit_block));
        add_edge(current, ctx->switch_context->exit_block);
    } else if (ctx->loop_context) {
        // Jump to loop exit
        add_instruction(current->instructions, create_ssa_jump(ctx->loop_context->exit));
        add_edge(current, ctx->loop_context->exit);
    }
    
    return NULL; // No successor block after break
}

BasicBlock* process_continue_statement(CFGBuilderContext* ctx, ContinueNode* continue_stmt, BasicBlock* current) {
    // Continue statements only make sense in loop contexts
    if (ctx->loop_context) {
        // Jump to loop header (which contains the loop condition)
        add_instruction(current->instructions, create_ssa_jump(ctx->loop_context->header));
        add_edge(current, ctx->loop_context->header);
    }
    // Note: continue in switch context is not valid C, but if it happens, we ignore it
    
    return NULL; // No successor block after continue
}

BasicBlock* process_switch_statement(CFGBuilderContext* ctx, SwitchNode* switch_stmt, BasicBlock* current) {
    // Evaluate switch expression
    SSAValue* switch_expr = process_expression(ctx, switch_stmt->expression, current);
    
    // Create blocks
    BasicBlock* exit_block = create_basic_block(ctx->cfg, "switch.exit");
    BasicBlock* default_block = NULL;
    
    // Count regular cases (non-default) and find default case
    int case_count = 0;
    for (int i = 0; i < switch_stmt->cases->count; i++) {
        CaseNode* case_node = (CaseNode*)switch_stmt->cases->items[i];
        if (case_node->value == NULL) {
            // This is a default case
            if (!default_block) {
                default_block = create_basic_block(ctx->cfg, "switch.default");
            }
        } else {
            // This is a regular case
            case_count++;
        }
    }
    
    // If no default case, create one that jumps to exit
    if (!default_block) {
        default_block = create_basic_block(ctx->cfg, "switch.default");
        add_instruction(default_block->instructions, create_ssa_jump(exit_block));
        add_edge(default_block, exit_block);
    }
    
    // Allocate case mappings
    SwitchCase* cases = NULL;
    if (case_count > 0) {
        cases = (SwitchCase*)malloc(case_count * sizeof(SwitchCase));
    }
    
    // Push switch context for break statements
    struct SwitchContext* switch_ctx = (struct SwitchContext*)malloc(sizeof(struct SwitchContext));
    switch_ctx->exit_block = exit_block;
    switch_ctx->parent = ctx->switch_context;
    ctx->switch_context = switch_ctx;
    
    // First pass: Create all case blocks and build case mappings
    BasicBlock** case_blocks = (BasicBlock**)malloc(switch_stmt->cases->count * sizeof(BasicBlock*));
    int case_index = 0;
    int default_index = -1;
    
    for (int i = 0; i < switch_stmt->cases->count; i++) {
        CaseNode* case_node = (CaseNode*)switch_stmt->cases->items[i];
        
        if (case_node->value == NULL) {
            // Default case
            case_blocks[i] = default_block;
            default_index = i;
        } else {
            // Regular case
            char block_name[64];
            snprintf(block_name, sizeof(block_name), "switch.case.%d", case_index);
            case_blocks[i] = create_basic_block(ctx->cfg, block_name);
            
            // Add to case mappings
            cases[case_index].case_value = atoi(((NumberLiteralNode*)case_node->value)->value);
            cases[case_index].target_block = case_blocks[i];
            case_index++;
        }
    }
    
    // Second pass: Process case bodies with fall-through support
    BasicBlock* last_block = NULL;
    
    for (int i = 0; i < switch_stmt->cases->count; i++) {
        CaseNode* case_node = (CaseNode*)switch_stmt->cases->items[i];
        BasicBlock* case_block = case_blocks[i];
        BasicBlock* current_block = case_block;
        
        // Process case body
        if (case_node->body && case_node->body->count > 0) {
            for (int j = 0; j < case_node->body->count; j++) {
                current_block = process_statement(ctx, case_node->body->items[j], current_block);
                if (!current_block) break; // Hit a break/return
            }
        }
        
        // Handle fall-through: if current_block is not NULL, it means no break/return
        if (current_block) {
            // Find next case to fall through to
            BasicBlock* next_case = NULL;
            for (int k = i + 1; k < switch_stmt->cases->count; k++) {
                next_case = case_blocks[k];
                break; // Fall through to the very next case
            }
            
            if (next_case) {
                // Fall through to next case
                add_instruction(current_block->instructions, create_ssa_jump(next_case));
                add_edge(current_block, next_case);
            } else {
                // No more cases, fall through to exit
                add_instruction(current_block->instructions, create_ssa_jump(exit_block));
                add_edge(current_block, exit_block);
            }
        }
        
        last_block = current_block;
    }
    
    // Clean up
    free(case_blocks);
    
    // Create switch instruction
    SSAInstruction* switch_inst = create_ssa_switch(switch_expr, cases, case_count, default_block);
    add_instruction(current->instructions, switch_inst);
    
    // Add edges from current block to all case blocks and default
    for (int i = 0; i < case_count; i++) {
        add_edge(current, cases[i].target_block);
    }
    add_edge(current, default_block);
    
    
    // Pop switch context
    ctx->switch_context = switch_ctx->parent;
    free(switch_ctx);
    
    return exit_block;
}

// --- Expression Processing ---

SSAValue* process_expression(CFGBuilderContext* ctx, Node* expr, BasicBlock* current) {
    if (!expr) return NULL;
    
    switch (expr->type) {
        case NODE_BINARY_OP:
            return process_binary_op(ctx, (BinaryOpNode*)expr, current);
        case NODE_ASSIGNMENT:
            return process_assignment(ctx, (AssignmentNode*)expr, current);
        case NODE_IDENTIFIER:
            return process_identifier(ctx, (IdentifierNode*)expr, current);
        case NODE_NUMBER_LITERAL:
            return process_number_literal(ctx, (NumberLiteralNode*)expr, current);
        case NODE_FUNCTION_CALL:
            return process_function_call(ctx, (FunctionCallNode*)expr, current);
        case NODE_ARRAY_ACCESS:
            return process_array_access(ctx, (ArrayAccessNode*)expr, current);
        case NODE_BOOL_LITERAL:
            return process_bool_literal(ctx, (BoolLiteralNode*)expr, current);
        case NODE_INITIALIZER_LIST:
            return process_initializer_list(ctx, (InitializerListNode*)expr, current);
        default:
            return NULL;
    }
}

SSAValue* process_binary_op(CFGBuilderContext* ctx, BinaryOpNode* bin_op, BasicBlock* current) {
    SSAValue* left = process_expression(ctx, bin_op->left, current);
    SSAValue* right = process_expression(ctx, bin_op->right, current);
    
    // Create temporary for result
    SSAValue* result = create_ssa_temp(ctx->next_temp_id++);
    
    SSAInstruction* inst = create_ssa_binary_op(result, bin_op->op, left, right);
    add_instruction(current->instructions, inst);
    
    return result;
}

SSAValue* process_assignment(CFGBuilderContext* ctx, AssignmentNode* assign, BasicBlock* current) {
    // Get variable name from identifier
    if (assign->identifier->type != NODE_IDENTIFIER) return NULL;
    IdentifierNode* ident = (IdentifierNode*)assign->identifier;
    
    // Process right-hand side
    SSAValue* value = process_expression(ctx, assign->value, current);
    
    // Create new version of variable
    int version = increment_var_version(ctx, ident->name);
    SSAValue* dest = create_ssa_var(ident->name, version);
    
    SSAInstruction* inst = create_ssa_assign(dest, value);
    add_instruction(current->instructions, inst);
    
    return dest;
}

SSAValue* process_identifier(CFGBuilderContext* ctx, IdentifierNode* ident, BasicBlock* current) {
    return get_current_var_value(ctx, ident->name);
}

SSAValue* process_number_literal(CFGBuilderContext* ctx, NumberLiteralNode* num, BasicBlock* current) {
    return create_ssa_const(atoi(num->value));
}

SSAValue* process_function_call(CFGBuilderContext* ctx, FunctionCallNode* call, BasicBlock* current) {
    // Process arguments
    SSAValue** args = NULL;
    if (call->arguments->count > 0) {
        args = (SSAValue**)malloc(call->arguments->count * sizeof(SSAValue*));
        for (int i = 0; i < call->arguments->count; i++) {
            args[i] = process_expression(ctx, call->arguments->items[i], current);
        }
    }
    
    // Create temporary for result
    SSAValue* result = create_ssa_temp(ctx->next_temp_id++);
    
    SSAInstruction* inst = create_ssa_call(result, call->name, args, call->arguments->count);
    add_instruction(current->instructions, inst);
    
    if (args) free(args);
    
    return result;
}

SSAValue* process_array_access(CFGBuilderContext* ctx, ArrayAccessNode* arr_access, BasicBlock* current) {
    // Process the array expression (usually an identifier)
    SSAValue* array = process_expression(ctx, arr_access->array, current);
    
    // Process the index expression
    SSAValue* index = process_expression(ctx, arr_access->index, current);
    
    // Create a temporary for the result
    SSAValue* result = create_ssa_temp(ctx->next_temp_id++);
    
    // For now, we'll create a LOAD instruction (in a full implementation,
    // this would generate proper array access code)
    SSAInstruction* inst = create_ssa_binary_op(result, TOKEN_LBRACKET, array, index);
    add_instruction(current->instructions, inst);
    
    return result;
}

SSAValue* process_bool_literal(CFGBuilderContext* ctx, BoolLiteralNode* bool_lit, BasicBlock* current) {
    (void)ctx; // Unused
    (void)current; // Unused
    return create_ssa_const(bool_lit->value);
}

SSAValue* process_initializer_list(CFGBuilderContext* ctx, InitializerListNode* init_list, BasicBlock* current) {
    // For now, we'll create a temporary array value and generate assignment instructions
    // for each element. In a more complete implementation, this would involve
    // memory allocation and proper array initialization.
    
    // Create a temporary for the array
    SSAValue* array_temp = create_ssa_temp(ctx->next_temp_id++);
    
    // Process each element and generate assignment instructions
    for (int i = 0; i < init_list->elements->count; i++) {
        SSAValue* element_value = process_expression(ctx, init_list->elements->items[i], current);
        
        // For now, we'll create a simple assignment instruction to represent
        // the array element initialization. In a more complete implementation,
        // this would be array store operations.
        SSAValue* element_temp = create_ssa_temp(ctx->next_temp_id++);
        SSAInstruction* assign = create_ssa_assign(element_temp, element_value);
        add_instruction(current->instructions, assign);
    }
    
    return array_temp;
}

// --- Helper Functions ---

void finalize_block(BasicBlock* block) {
    // Add any necessary terminator instruction if missing
    if (block->instructions->count == 0 || 
        (block->instructions->items[block->instructions->count - 1]->type != SSA_JUMP &&
         block->instructions->items[block->instructions->count - 1]->type != SSA_BRANCH &&
         block->instructions->items[block->instructions->count - 1]->type != SSA_RETURN)) {
        // Block needs a terminator
        if (block->successor_count == 1) {
            add_instruction(block->instructions, create_ssa_jump(block->successors[0]));
        }
    }
}

BasicBlock* split_block(CFGBuilderContext* ctx, BasicBlock* block, const char* label) {
    // Create new block
    BasicBlock* new_block = create_basic_block(ctx->cfg, label);
    
    // Move successors to new block
    new_block->successors = block->successors;
    new_block->successor_count = block->successor_count;
    new_block->successor_capacity = block->successor_capacity;
    
    // Update predecessors of successors
    for (int i = 0; i < new_block->successor_count; i++) {
        BasicBlock* succ = new_block->successors[i];
        for (int j = 0; j < succ->predecessor_count; j++) {
            if (succ->predecessors[j] == block) {
                succ->predecessors[j] = new_block;
            }
        }
    }
    
    // Reset block's successors
    block->successors = NULL;
    block->successor_count = 0;
    block->successor_capacity = 0;
    
    // Add edge from block to new_block
    add_edge(block, new_block);
    
    return new_block;
}
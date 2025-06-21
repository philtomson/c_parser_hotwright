#define _GNU_SOURCE  // For strdup
#include "hw_analyzer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// --- Internal Helper Functions ---

static void init_hardware_context(HardwareContext* ctx);
static void resize_state_array(HardwareContext* ctx);
static void resize_input_array(HardwareContext* ctx);
static void add_state_variable(HardwareContext* ctx, VarDeclNode* var_decl);
static void add_input_variable(HardwareContext* ctx, VarDeclNode* var_decl);
static void build_lookup_tables(HardwareContext* ctx);
static void traverse_ast_for_variables(Node* node, HardwareContext* ctx);
static bool extract_initial_bool_value(Node* initializer);

// --- Main Analysis Function ---

HardwareContext* analyze_hardware_constructs(Node* ast) {
    if (!ast) {
        return NULL;
    }
    
    HardwareContext* ctx = malloc(sizeof(HardwareContext));
    init_hardware_context(ctx);
    
    // Traverse AST to find all variable declarations
    traverse_ast_for_variables(ast, ctx);
    
    // Build lookup tables for fast access
    build_lookup_tables(ctx);
    
    // Validate the hardware context
    ctx->analysis_successful = validate_hardware_context(ctx);
    
    return ctx;
}

// --- Variable Classification ---

HardwareVarType classify_variable(VarDeclNode* var_decl) {
    if (!var_decl || var_decl->base.type != NODE_VAR_DECL) {
        return HW_VAR_UNKNOWN;
    }
    
    // Must be boolean or integer type (for hardware variables)
    if (var_decl->var_type != TOKEN_BOOL && var_decl->var_type != TOKEN_INT) {
        return HW_VAR_UNKNOWN;
    }
    
    // Check for state variable pattern
    if (is_state_variable(var_decl)) {
        return HW_VAR_STATE;
    }
    
    // Check for input variable pattern
    if (is_input_variable(var_decl)) {
        return HW_VAR_INPUT;
    }
    
    return HW_VAR_UNKNOWN;
}

bool is_state_variable(VarDeclNode* var_decl) {
    // State variables:
    // 1. Must be boolean or integer type
    // 2. Must have initialization
    // 3. Must follow LED naming pattern (for now, until comment support)
    return (var_decl->var_type == TOKEN_BOOL || var_decl->var_type == TOKEN_INT) &&
           (var_decl->initializer != NULL) &&
           is_led_variable(var_decl->var_name);
}

bool is_input_variable(VarDeclNode* var_decl) {
    // Input variables:
    // 1. Must be boolean or integer type
    // 2. Must NOT have initialization
    // 3. Must follow input naming pattern (a0, a1, etc.)
    return (var_decl->var_type == TOKEN_BOOL || var_decl->var_type == TOKEN_INT) &&
           (var_decl->initializer == NULL) &&
           (strlen(var_decl->var_name) >= 2) &&
           (var_decl->var_name[0] == 'a') &&
           isdigit(var_decl->var_name[1]);
}

// --- Hardware Pattern Recognition ---

bool is_led_variable(const char* var_name) {
    // Check for LED naming pattern: LED0, LED1, LED2, etc.
    if (!var_name || strlen(var_name) < 4) {
        return false;
    }
    
    return (strncmp(var_name, "LED", 3) == 0) && isdigit(var_name[3]);
}

int extract_state_number_from_name(const char* var_name) {
    // Extract state number from LED variable name
    // LED0 -> 0, LED1 -> 1, etc.
    if (!is_led_variable(var_name)) {
        return -1;
    }
    
    return atoi(var_name + 3);  // Skip "LED" prefix
}

// --- AST Traversal ---

static void traverse_ast_for_variables(Node* node, HardwareContext* ctx) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_PROGRAM: {
            ProgramNode* prog = (ProgramNode*)node;
            for (int i = 0; i < prog->functions->count; i++) {
                traverse_ast_for_variables(prog->functions->items[i], ctx);
            }
            break;
        }
        
        case NODE_FUNCTION_DEF: {
            FunctionDefNode* func = (FunctionDefNode*)node;
            traverse_ast_for_variables(func->body, ctx);
            break;
        }
        
        case NODE_BLOCK: {
            BlockNode* block = (BlockNode*)node;
            for (int i = 0; i < block->statements->count; i++) {
                traverse_ast_for_variables(block->statements->items[i], ctx);
            }
            break;
        }
        
        case NODE_VAR_DECL: {
            VarDeclNode* var_decl = (VarDeclNode*)node;
            HardwareVarType type = classify_variable(var_decl);
            
            if (type == HW_VAR_STATE) {
                add_state_variable(ctx, var_decl);
            } else if (type == HW_VAR_INPUT) {
                add_input_variable(ctx, var_decl);
            }
            break;
        }
        
        case NODE_IF: {
            IfNode* if_node = (IfNode*)node;
            traverse_ast_for_variables(if_node->then_branch, ctx);
            if (if_node->else_branch) {
                traverse_ast_for_variables(if_node->else_branch, ctx);
            }
            break;
        }
        
        case NODE_WHILE: {
            WhileNode* while_node = (WhileNode*)node;
            traverse_ast_for_variables(while_node->body, ctx);
            break;
        }
        
        case NODE_FOR: {
            ForNode* for_node = (ForNode*)node;
            if (for_node->init) {
                traverse_ast_for_variables(for_node->init, ctx);
            }
            traverse_ast_for_variables(for_node->body, ctx);
            break;
        }
        
        default:
            // For other node types, we don't need to traverse further
            // for variable declarations
            break;
    }
}

// --- Helper Functions ---

static void init_hardware_context(HardwareContext* ctx) {
    ctx->states = malloc(sizeof(StateVariable) * 8);
    ctx->state_count = 0;
    ctx->state_capacity = 8;
    
    ctx->inputs = malloc(sizeof(InputVariable) * 8);
    ctx->input_count = 0;
    ctx->input_capacity = 8;
    
    ctx->all_var_names = NULL;
    ctx->var_types = NULL;
    ctx->total_var_count = 0;
    
    ctx->analysis_successful = false;
    ctx->error_message = NULL;
}

static void add_state_variable(HardwareContext* ctx, VarDeclNode* var_decl) {
    if (ctx->state_count >= ctx->state_capacity) {
        resize_state_array(ctx);
    }
    
    StateVariable* state = &ctx->states[ctx->state_count];
    state->name = strdup(var_decl->var_name);
    state->state_number = extract_state_number_from_name(var_decl->var_name);
    state->initial_value = extract_initial_bool_value(var_decl->initializer);
    state->ast_node = var_decl;
    
    ctx->state_count++;
}

static void add_input_variable(HardwareContext* ctx, VarDeclNode* var_decl) {
    if (ctx->input_count >= ctx->input_capacity) {
        resize_input_array(ctx);
    }
    
    InputVariable* input = &ctx->inputs[ctx->input_count];
    input->name = strdup(var_decl->var_name);
    input->input_number = ctx->input_count;  // Auto-assign sequential numbers
    input->ast_node = var_decl;
    
    ctx->input_count++;
}

static bool extract_initial_bool_value(Node* initializer) {
    if (!initializer) return false;
    
    if (initializer->type == NODE_BOOL_LITERAL) {
        BoolLiteralNode* bool_lit = (BoolLiteralNode*)initializer;
        return bool_lit->value != 0;
    }
    
    if (initializer->type == NODE_NUMBER_LITERAL) {
        NumberLiteralNode* num_lit = (NumberLiteralNode*)initializer;
        return atoi(num_lit->value) != 0;
    }
    
    return false;
}

static void resize_state_array(HardwareContext* ctx) {
    ctx->state_capacity *= 2;
    ctx->states = realloc(ctx->states, sizeof(StateVariable) * ctx->state_capacity);
}

static void resize_input_array(HardwareContext* ctx) {
    ctx->input_capacity *= 2;
    ctx->inputs = realloc(ctx->inputs, sizeof(InputVariable) * ctx->input_capacity);
}

// --- Lookup Functions ---

int get_state_number_by_name(HardwareContext* ctx, const char* var_name) {
    for (int i = 0; i < ctx->state_count; i++) {
        if (strcmp(ctx->states[i].name, var_name) == 0) {
            return ctx->states[i].state_number;
        }
    }
    return -1;
}

int get_input_number_by_name(HardwareContext* ctx, const char* var_name) {
    for (int i = 0; i < ctx->input_count; i++) {
        if (strcmp(ctx->inputs[i].name, var_name) == 0) {
            return ctx->inputs[i].input_number;
        }
    }
    return -1;
}

HardwareVarType get_variable_type(HardwareContext* ctx, const char* var_name) {
    if (get_state_number_by_name(ctx, var_name) >= 0) {
        return HW_VAR_STATE;
    }
    if (get_input_number_by_name(ctx, var_name) >= 0) {
        return HW_VAR_INPUT;
    }
    return HW_VAR_UNKNOWN;
}

// --- Validation ---

bool validate_hardware_context(HardwareContext* ctx) {
    return check_state_number_conflicts(ctx) && 
           check_variable_name_conflicts(ctx);
}

bool check_state_number_conflicts(HardwareContext* ctx) {
    // Check for duplicate state numbers
    for (int i = 0; i < ctx->state_count; i++) {
        for (int j = i + 1; j < ctx->state_count; j++) {
            if (ctx->states[i].state_number == ctx->states[j].state_number) {
                ctx->error_message = strdup("Duplicate state numbers detected");
                return false;
            }
        }
    }
    return true;
}

bool check_variable_name_conflicts(HardwareContext* ctx) {
    // Check for duplicate variable names between states and inputs
    for (int i = 0; i < ctx->state_count; i++) {
        for (int j = 0; j < ctx->input_count; j++) {
            if (strcmp(ctx->states[i].name, ctx->inputs[j].name) == 0) {
                ctx->error_message = strdup("Variable name conflict between state and input");
                return false;
            }
        }
    }
    return true;
}

// --- Output Functions ---

void print_hardware_context(HardwareContext* ctx, FILE* output) {
    fprintf(output, "\n=== Hardware Analysis Results ===\n");
    fprintf(output, "Analysis successful: %s\n", ctx->analysis_successful ? "Yes" : "No");
    
    if (ctx->error_message) {
        fprintf(output, "Error: %s\n", ctx->error_message);
    }
    
    print_state_variables(ctx, output);
    print_input_variables(ctx, output);
}

void print_state_variables(HardwareContext* ctx, FILE* output) {
    fprintf(output, "\nState Variables (Outputs):\n");
    for (int i = 0; i < ctx->state_count; i++) {
        StateVariable* state = &ctx->states[i];
        fprintf(output, "  %s -> state%d (initial: %d)\n", 
                state->name, state->state_number, state->initial_value);
    }
}

void print_input_variables(HardwareContext* ctx, FILE* output) {
    fprintf(output, "\nInput Variables:\n");
    for (int i = 0; i < ctx->input_count; i++) {
        InputVariable* input = &ctx->inputs[i];
        fprintf(output, "  %s -> input%d\n", 
                input->name, input->input_number);
    }
}

// --- Memory Management ---

void free_hardware_context(HardwareContext* ctx) {
    if (!ctx) return;
    
    // Free state variables
    for (int i = 0; i < ctx->state_count; i++) {
        free(ctx->states[i].name);
    }
    free(ctx->states);
    
    // Free input variables
    for (int i = 0; i < ctx->input_count; i++) {
        free(ctx->inputs[i].name);
    }
    free(ctx->inputs);
    
    // Free lookup tables
    if (ctx->all_var_names) {
        for (int i = 0; i < ctx->total_var_count; i++) {
            free(ctx->all_var_names[i]);
        }
        free(ctx->all_var_names);
    }
    free(ctx->var_types);
    
    // Free error message
    free(ctx->error_message);
    
    free(ctx);
}

// --- Build Lookup Tables (placeholder for future optimization) ---

static void build_lookup_tables(HardwareContext* ctx) {
    // For now, we use the direct lookup functions
    // This can be optimized later with hash tables if needed
    ctx->total_var_count = ctx->state_count + ctx->input_count;
}
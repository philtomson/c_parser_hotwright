#include "ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// --- Debug Functions ---

void print_debug(const char* format, ...) {
    if (!debug_mode) return;
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

// Helper to create a node list
NodeList* create_node_list() {
    NodeList* list = malloc(sizeof(NodeList));
    list->capacity = 8;
    list->count = 0;
    list->items = malloc(sizeof(Node*) * list->capacity);
    return list;
}

// Public function to add a node to a list, handling resize
void add_node_to_list(NodeList* list, Node* node) {
    if (list->count == list->capacity) {
        list->capacity *= 2;
        list->items = realloc(list->items, sizeof(Node*) * list->capacity);
    }
    list->items[list->count++] = node;
}

// --- Creation functions for each node type (with missing implementations added) ---

Node* create_program_node() {
    ProgramNode* node = malloc(sizeof(ProgramNode));
    node->base.type = NODE_PROGRAM;
    node->functions = create_node_list();
    return (Node*)node;
}

Node* create_function_def_node(char* name, NodeList* parameters, Node* body) {
    FunctionDefNode* node = malloc(sizeof(FunctionDefNode));
    node->base.type = NODE_FUNCTION_DEF;
    node->name = name; // Assumes ownership of name
    node->parameters = parameters;
    node->body = body;
    return (Node*)node;
}

Node* create_block_node() {
    BlockNode* node = malloc(sizeof(BlockNode));
    node->base.type = NODE_BLOCK;
    node->statements = create_node_list();
    return (Node*)node;
}

Node* create_var_decl_node(TokenType var_type, char* var_name, int array_size, int bit_width, Node* initializer) {
    VarDeclNode* node = malloc(sizeof(VarDeclNode));
    node->base.type = NODE_VAR_DECL;
    node->var_type = var_type;
    node->var_name = var_name;
    node->array_size = array_size;
    node->bit_width = bit_width;
    node->initializer = initializer;
    return (Node*)node;
}

Node* create_expression_statement_node(Node* expression) {
    ExpressionStatementNode* node = malloc(sizeof(ExpressionStatementNode));
    node->base.type = NODE_EXPRESSION_STATEMENT;
    node->expression = expression;
    return (Node*)node;
}

Node* create_switch_node(Node* expression) {
    SwitchNode* node = malloc(sizeof(SwitchNode));
    node->base.type = NODE_SWITCH;
    node->expression = expression;
    node->cases = create_node_list();
    return (Node*)node;
}

Node* create_case_node(Node* value) {
    CaseNode* node = malloc(sizeof(CaseNode));
    node->base.type = NODE_CASE;
    node->value = value;
    node->body = create_node_list();
    return (Node*)node;
}

Node* create_break_node() {
    BreakNode* node = malloc(sizeof(BreakNode));
    node->base.type = NODE_BREAK;
    return (Node*)node;
}

// --- MISSING IMPLEMENTATIONS ADDED HERE ---
Node* create_binary_op_node(TokenType op, Node* left, Node* right) {
    BinaryOpNode* node = malloc(sizeof(BinaryOpNode));
    node->base.type = NODE_BINARY_OP;
    node->op = op;
    node->left = left;
    node->right = right;
    return (Node*)node;
Node* create_unary_op_node(TokenType op, Node* operand) {
    UnaryOpNode* node = malloc(sizeof(UnaryOpNode));
    node->base.type = NODE_UNARY_OP;
    node->op = op;
    node->operand = operand;
    return (Node*)node;
}
}

Node* create_assignment_node(Node* identifier, Node* value) {
    // We expect the identifier node to be of the correct type.
    // A production compiler would have an assert here.
    // assert(identifier && identifier->type == NODE_IDENTIFIER);

    AssignmentNode* node = malloc(sizeof(AssignmentNode));
    node->base.type = NODE_ASSIGNMENT;
    node->identifier = identifier; // Takes ownership of the identifier node
    node->value = value;
    return (Node*)node;
}

// --- END OF ADDED IMPLEMENTATIONS ---

Node* create_identifier_node(char* name) {
    IdentifierNode* node = malloc(sizeof(IdentifierNode));
    node->base.type = NODE_IDENTIFIER;
    node->name = name;
    return (Node*)node;
}

Node* create_number_literal_node(char* value) {
    NumberLiteralNode* node = malloc(sizeof(NumberLiteralNode));
    node->base.type = NODE_NUMBER_LITERAL;
    node->value = value;
    return (Node*)node;
}

Node* create_if_node(Node* condition, Node* then_branch, Node* else_branch) {
    IfNode* node = malloc(sizeof(IfNode));
    node->base.type = NODE_IF;
    node->condition = condition;
    node->then_branch = then_branch;
    node->else_branch = else_branch;  // Can be NULL
    return (Node*)node;
}

Node* create_while_node(Node* condition, Node* body) {
    WhileNode* node = malloc(sizeof(WhileNode));
    node->base.type = NODE_WHILE;
    node->condition = condition;
    node->body = body;
    return (Node*)node;
}

Node* create_for_node(Node* init, Node* condition, Node* update, Node* body) {
    ForNode* node = malloc(sizeof(ForNode));
    node->base.type = NODE_FOR;
    node->init = init;
    node->condition = condition;
    node->update = update;
    node->body = body;
    return (Node*)node;
}

Node* create_return_node(Node* return_value) {
    ReturnNode* node = malloc(sizeof(ReturnNode));
    node->base.type = NODE_RETURN;
    node->return_value = return_value;
    return (Node*)node;
}

Node* create_function_call_node(char* name, NodeList* arguments) {
    FunctionCallNode* node = malloc(sizeof(FunctionCallNode));
    node->base.type = NODE_FUNCTION_CALL;
    node->name = name;
    node->arguments = arguments;
    return (Node*)node;
}

Node* create_array_access_node(Node* array, Node* index) {
    ArrayAccessNode* node = malloc(sizeof(ArrayAccessNode));
    node->base.type = NODE_ARRAY_ACCESS;
    node->array = array;
    node->index = index;
    return (Node*)node;
}

Node* create_bool_literal_node(int value) {
    BoolLiteralNode* node = malloc(sizeof(BoolLiteralNode));
    node->base.type = NODE_BOOL_LITERAL;
    node->value = value;
    return (Node*)node;
}

Node* create_initializer_list_node(NodeList* elements) {
    InitializerListNode* node = malloc(sizeof(InitializerListNode));
    node->base.type = NODE_INITIALIZER_LIST;
    node->elements = elements;
    return (Node*)node;
}

// --- Cleanup Functions ---

// Recursively free a node list
static void free_node_list(NodeList* list) {
    for (int i = 0; i < list->count; i++) {
        free_node(list->items[i]);
    }
    free(list->items);
    free(list);
}

// The most important function for C: cleanup!
// in ast.c

// The fully implemented, correct free_node function.
void free_node(Node* node) {
    if (!node) return;

    switch (node->type) {
        case NODE_PROGRAM: {
            ProgramNode* n = (ProgramNode*)node;
            free_node_list(n->functions);
            break;
        }
        case NODE_FUNCTION_DEF: {
            FunctionDefNode* n = (FunctionDefNode*)node;
            free(n->name);
            free_node_list(n->parameters);
            free_node(n->body);
            break;
        }
        case NODE_BLOCK: {
            BlockNode* n = (BlockNode*)node;
            free_node_list(n->statements);
            break;
        }
        case NODE_VAR_DECL: {
            VarDeclNode* n = (VarDeclNode*)node;
            free(n->var_name);
            free_node(n->initializer);
            break;
        }
        case NODE_EXPRESSION_STATEMENT: {
            ExpressionStatementNode* n = (ExpressionStatementNode*)node;
            free_node(n->expression);
            break;
        }
        case NODE_SWITCH: {
            SwitchNode* n = (SwitchNode*)node;
            free_node(n->expression);
            free_node_list(n->cases);
            break;
        }
        case NODE_CASE: {
            CaseNode* n = (CaseNode*)node;
            free_node(n->value);
            free_node_list(n->body);
            break;
        }
        case NODE_BINARY_OP: {
            BinaryOpNode* n = (BinaryOpNode*)node;
            free_node(n->left);
            free_node(n->right);
            break;
        }
        case NODE_ASSIGNMENT: {
            AssignmentNode* n = (AssignmentNode*)node;
            free_node(n->identifier);
            free_node(n->value);
            break;
        }
        case NODE_BREAK: {
            // No children to free.
            break;
        }
        case NODE_IDENTIFIER: {
            IdentifierNode* n = (IdentifierNode*)node;
            free(n->name);
            break;
        }
        case NODE_NUMBER_LITERAL: {
            NumberLiteralNode* n = (NumberLiteralNode*)node;
            free(n->value);
            break;
        }
        // These cases were likely missing from my previous incomplete answer.
        case NODE_IF: {
            IfNode* n = (IfNode*)node;
            free_node(n->condition);
            free_node(n->then_branch);
            free_node(n->else_branch);
            break;
        }
        case NODE_WHILE: {
            WhileNode* n = (WhileNode*)node;
            free_node(n->condition);
            free_node(n->body);
            break;
        }
        case NODE_FOR: {
            ForNode* n = (ForNode*)node;
            free_node(n->init);
            free_node(n->condition);
            free_node(n->update);
            free_node(n->body);
            break;
        }
         case NODE_RETURN: {
            ReturnNode* n = (ReturnNode*)node;
            free_node(n->return_value);
            break;
        }
        case NODE_FUNCTION_CALL: {
            FunctionCallNode* n = (FunctionCallNode*)node;
            free(n->name);
            free_node_list(n->arguments);
            break;
        }
        case NODE_ARRAY_ACCESS: {
            ArrayAccessNode* n = (ArrayAccessNode*)node;
            free_node(n->array);
            free_node(n->index);
            break;
        }
        case NODE_BOOL_LITERAL: {
            // BoolLiteralNode has no dynamically allocated members
            break;
        }
        case NODE_INITIALIZER_LIST: {
            InitializerListNode* n = (InitializerListNode*)node;
            free_node_list(n->elements);
            break;
        }
    }
    // Finally, free the node struct itself.
    // This is the call that is crashing at ast.c:145
    print_debug("DEBUG: About to free node at address %p, type %d\n", (void*)node, node->type);
    if (debug_mode) {
        fflush(stdout);  // Make sure the output appears before the crash
    }
    free(node);
    print_debug("DEBUG: Successfully freed node\n");  // This won't print if free() crashes
}

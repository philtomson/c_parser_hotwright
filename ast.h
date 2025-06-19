#ifndef AST_H
#define AST_H

#include "lexer.h"

typedef enum {
    NODE_PROGRAM,
    NODE_FUNCTION_DEF,
    NODE_BLOCK,
    NODE_VAR_DECL,
    NODE_EXPRESSION_STATEMENT,
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,
    NODE_SWITCH,
    NODE_CASE,
    NODE_RETURN,
    NODE_BREAK,
    NODE_BINARY_OP,        // <-- For expressions like a + b
    NODE_ASSIGNMENT,       // <-- For expressions like x = 5
    NODE_FUNCTION_CALL,    // <-- For function calls like f(x, y)
    NODE_IDENTIFIER,
    NODE_NUMBER_LITERAL
} NodeType;

// Base struct for all AST nodes
typedef struct Node {
    NodeType type;
} Node;

// A dynamic array of nodes
typedef struct {
    Node** items;
    int count;
    int capacity;
} NodeList;

// --- Specific Node Structures ---

typedef struct {
    Node base;
    NodeList* functions;
} ProgramNode;

typedef struct {
    Node base;
    char* name;
    NodeList* parameters; // List of IdentifierNodes
    Node* body; // BlockNode
} FunctionDefNode;

typedef struct {
    Node base;
    NodeList* statements;
} BlockNode;

typedef struct {
    Node base;
    char* var_name;
    Node* initializer; // Can be NULL
} VarDeclNode;

typedef struct {
    Node base;
    Node* expression;
} ExpressionStatementNode;

typedef struct {
    Node base;
    Node* condition;
    Node* then_branch;
    Node* else_branch; // Can be NULL
} IfNode;

typedef struct {
    Node base;
    Node* condition;
    Node* body;
} WhileNode;

typedef struct {
    Node base;
    Node* init;      // Can be NULL
    Node* condition; // Can be NULL
    Node* update;    // Can be NULL
    Node* body;
} ForNode;

typedef struct {
    Node base;
    Node* expression;
    NodeList* cases;
} SwitchNode;

typedef struct {
    Node base;
    Node* value; // The expression after 'case', NULL for 'default'
    NodeList* body;
} CaseNode;

typedef struct {
    Node base;
    Node* return_value;
} ReturnNode;

typedef struct {
    Node base; // A simple node, no extra data for break
} BreakNode;

typedef struct {
    Node base;
    TokenType op;
    Node* left;
    Node* right;
} BinaryOpNode;

/*
typedef struct {
    Node base;
    char* identifier_name; // Changed from 'identifier' to avoid conflict with node type
    Node* value;
} AssignmentNode;
*/

typedef struct {
    Node base;
    Node* identifier; // <-- CHANGE THIS: from char* to Node*
    Node* value;
} AssignmentNode;

typedef struct {
    Node base;
    char* name;
} IdentifierNode;

typedef struct {
    Node base;
    char* value;
} NumberLiteralNode;

typedef struct {
    Node base;
    char* name;
    NodeList* arguments;
} FunctionCallNode;


// --- AST Creation Functions (with missing declarations added) ---
Node* create_program_node();
Node* create_function_def_node(char* name, NodeList* parameters, Node* body);
Node* create_block_node();
Node* create_var_decl_node(char* var_name, Node* initializer);
Node* create_expression_statement_node(Node* expression);
Node* create_switch_node(Node* expression);
Node* create_case_node(Node* value);
Node* create_break_node();
Node* create_binary_op_node(TokenType op, Node* left, Node* right); // <-- MISSING
//Node* create_assignment_node(char* name, Node* value);         // <-- MISSING
Node* create_assignment_node(Node* identifier, Node* value);
Node* create_identifier_node(char* name);
Node* create_number_literal_node(char* value);
Node* create_if_node(Node* condition, Node* then_branch, Node* else_branch);
Node* create_while_node(Node* condition, Node* body);
Node* create_for_node(Node* init, Node* condition, Node* update, Node* body);
Node* create_return_node(Node* return_value);
Node* create_function_call_node(char* name, NodeList* arguments);
// ... add others as you expand the language


// --- AST Management ---
NodeList* create_node_list();
void add_node_to_list(NodeList* list, Node* node);
void free_node(Node* node);

#endif // AST_H

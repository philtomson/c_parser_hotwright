#define _GNU_SOURCE  // For strdup
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h> // For strdup

// --- Forward declarations for the recursive functions ---
// These are internal to the parser module, so they are static.

// Statement Parsers
static Node* parse_statement(Parser* p);
static Node* parse_function_definition(Parser* p);
static Node* parse_block_statement(Parser* p);
static Node* parse_declaration_statement(Parser* p);
static Node* parse_if_statement(Parser* p);
static Node* parse_switch_statement(Parser* p);
static Node* parse_expression_statement(Parser* p);

// Expression Parsers (for operator precedence)
static Node* parse_expression(Parser* p);
static Node* parse_assignment(Parser* p);
static Node* parse_equality(Parser* p);
static Node* parse_relational(Parser* p);
static Node* parse_additive(Parser* p);
static Node* parse_multiplicative(Parser* p);
static Node* parse_primary(Parser* p);

static void debug_print_node(Node* node, const char* context) {
    if (!node) {
        printf("DEBUG: %s - NULL node\n", context);
        return;
    }
    printf("DEBUG: %s - Node type %d at address %p\n", context, node->type, (void*)node);
    if (node->type == NODE_IDENTIFIER) {
        IdentifierNode* id = (IdentifierNode*)node;
        printf("  Identifier name: '%s' at address %p\n", id->name, (void*)id->name);
    }
}


// --- Helper Functions ---

static void parser_error(const char* message) {
    fprintf(stderr, "Parse Error: %s\n", message);
    exit(1);
}

static Token current_token(Parser* p) { return p->tokens[p->pos]; }
static Token peek_token(Parser* p) {
    if (p->pos + 1 < p->count) {
        return p->tokens[p->pos + 1];
    }
    return p->tokens[p->count - 1]; // Return EOF if at the end
}
static void advance(Parser* p) { if (p->pos < p->count) p->pos++; }

static int match(Parser* p, TokenType type) {
    if (current_token(p).type == type) {
        advance(p);
        return 1;
    }
    return 0;
}

static Token expect(Parser* p, TokenType type, const char* msg) {
    Token token = current_token(p);
    if (token.type == type) {
        advance(p);
        return token;
    }
    fprintf(stderr, "Parse Error: %s. Got %s ('%s') instead.\n",
            msg, token_type_to_string(token.type), token.value);
    exit(1);
}

// --- Expression Parsing Logic ---
// This chain of functions implements operator precedence.
// Each function handles one level of precedence and calls the next higher level.





static Node* parse_multiplicative(Parser* p) {
    Node* node = parse_primary(p);
    while (current_token(p).type == TOKEN_STAR || current_token(p).type == TOKEN_SLASH) {
        // Get the operator type BEFORE advancing.
        TokenType op = current_token(p).type;
        advance(p); // Manually advance instead of using match()
        Node* right = parse_primary(p);
        node = create_binary_op_node(op, node, right);
    }
    return node;
}

static Node* parse_primary(Parser* p) {
    if (current_token(p).type == TOKEN_NUMBER) {
        char* value = strdup(current_token(p).value);
        if (!value) {
            parser_error("Memory allocation failed for number literal");
        }
        advance(p);
        Node* node = create_number_literal_node(value);
        debug_print_node(node, "Created number literal");
        return node;
    }
    
    if (current_token(p).type == TOKEN_IDENTIFIER) {
        char* value = strdup(current_token(p).value);
        if (!value) {
            parser_error("Memory allocation failed for identifier");
        }
        advance(p);
        Node* node = create_identifier_node(value);
        debug_print_node(node, "Created identifier");
        return node;
    }
    
    if (match(p, TOKEN_LPAREN)) {
        Node* expr = parse_expression(p);
        expect(p, TOKEN_RPAREN, "Expected ')' after expression");
        return expr;
    }

    parser_error("Unexpected token in expression");
    return NULL;
}

static Node* parse_additive(Parser* p) {
    Node* node = parse_multiplicative(p);
    while (current_token(p).type == TOKEN_PLUS || current_token(p).type == TOKEN_MINUS) {
        TokenType op = current_token(p).type;
        advance(p);
        Node* right = parse_multiplicative(p);
        node = create_binary_op_node(op, node, right);
    }
    return node;
}


static Node* parse_relational(Parser* p) {
    Node* node = parse_additive(p);
     while (current_token(p).type == TOKEN_LESS || current_token(p).type == TOKEN_LESS_EQUAL || current_token(p).type == TOKEN_GREATER || current_token(p).type == TOKEN_GREATER_EQUAL) {
        TokenType op = current_token(p).type;
        advance(p);
        Node* right = parse_additive(p);
        node = create_binary_op_node(op, node, right);
    }
    return node;
}


static Node* parse_equality(Parser* p) {
    Node* node = parse_relational(p);
    while (current_token(p).type == TOKEN_EQUAL || current_token(p).type == TOKEN_NOT_EQUAL) {
        TokenType op = current_token(p).type;
        advance(p);
        Node* right = parse_relational(p);
        node = create_binary_op_node(op, node, right);
    }
    return node;
}





static Node* parse_expression(Parser* p) {
    return parse_assignment(p);
}

static Node* parse_assignment(Parser* p) {
    Node* left = parse_equality(p);
    
    if (current_token(p).type == TOKEN_ASSIGN) {
        advance(p);
        
        if (!left) {
            parser_error("Missing left-hand side of assignment.");
        }

        if (left->type != NODE_IDENTIFIER) {
            parser_error("Invalid assignment target. Must be an identifier.");
        }
        
        debug_print_node(left, "Assignment LHS");
        
        Node* value = parse_assignment(p);
        if (!value) {
            parser_error("Missing right-hand side of assignment.");
        }
        
        debug_print_node(value, "Assignment RHS");
        
        Node* assignment = create_assignment_node(left, value);
        debug_print_node(assignment, "Created assignment");
        return assignment;
    }

    return left;
}



// --- Statement Parsing Logic ---

static Node* parse_expression_statement(Parser* p) {
    Node* expr = parse_expression(p);
    expect(p, TOKEN_SEMICOLON, "Expected ';' after expression");
    return create_expression_statement_node(expr);
}

static Node* parse_declaration_statement(Parser* p) {
    expect(p, TOKEN_INT, "Expected 'int' for declaration");
    Token id_tok = expect(p, TOKEN_IDENTIFIER, "Expected identifier in declaration");
    
    Node* initializer = NULL;
    if (match(p, TOKEN_ASSIGN)) {
        initializer = parse_expression(p);
    }
    expect(p, TOKEN_SEMICOLON, "Expected ';' after declaration");
    return create_var_decl_node(strdup(id_tok.value), initializer);
}

static Node* parse_switch_statement(Parser* p) {
    expect(p, TOKEN_SWITCH, "Expected 'switch'");
    expect(p, TOKEN_LPAREN, "Expected '(' after 'switch'");
    Node* condition = parse_expression(p);
    expect(p, TOKEN_RPAREN, "Expected ')' after switch condition");
    expect(p, TOKEN_LBRACE, "Expected '{' for switch body");

    SwitchNode* switch_node = (SwitchNode*)create_switch_node(condition);

    while (current_token(p).type != TOKEN_RBRACE && current_token(p).type != TOKEN_EOF) {
        Node* case_stmt;
        if (match(p, TOKEN_CASE)) {
            Node* value = parse_expression(p);
            expect(p, TOKEN_COLON, "Expected ':' after case value");
            case_stmt = create_case_node(value);
        } else if (match(p, TOKEN_DEFAULT)) {
            expect(p, TOKEN_COLON, "Expected ':' after default");
            case_stmt = create_case_node(NULL); // NULL value for default
        } else {
            parser_error("Expected 'case' or 'default' inside switch");
            return NULL; // Unreachable
        }
        
        // Parse statements for this case until the next case, default, or end of block
        while (current_token(p).type != TOKEN_CASE &&
               current_token(p).type != TOKEN_DEFAULT &&
               current_token(p).type != TOKEN_RBRACE) {
            add_node_to_list(((CaseNode*)case_stmt)->body, parse_statement(p));
        }
        add_node_to_list(switch_node->cases, case_stmt);
    }

    expect(p, TOKEN_RBRACE, "Expected '}' to close switch body");
    return (Node*)switch_node;
}

static Node* parse_if_statement(Parser* p) {
    expect(p, TOKEN_IF, "Expected 'if'");
    expect(p, TOKEN_LPAREN, "Expected '(' after 'if'");
    Node* condition = parse_expression(p);
    expect(p, TOKEN_RPAREN, "Expected ')' after if condition");
    
    Node* then_branch = parse_statement(p);
    Node* else_branch = NULL;
    
    if (match(p, TOKEN_ELSE)) {
        else_branch = parse_statement(p);
    }
    
    return create_if_node(condition, then_branch, else_branch);
}

static Node* parse_statement(Parser* p) {
    // Check for declaration: 'int' followed by an identifier
    if (current_token(p).type == TOKEN_INT && peek_token(p).type == TOKEN_IDENTIFIER) {
        return parse_declaration_statement(p);
    }

    switch (current_token(p).type) {
        case TOKEN_IF: return parse_if_statement(p);
        case TOKEN_SWITCH: return parse_switch_statement(p);
        case TOKEN_BREAK:
            advance(p);
            expect(p, TOKEN_SEMICOLON, "Expected ';' after break");
            return create_break_node();
        case TOKEN_LBRACE: return parse_block_statement(p);
        
        // Add other statement types like while, return here
        
        default: return parse_expression_statement(p);
    }
}

static Node* parse_block_statement(Parser* p) {
    expect(p, TOKEN_LBRACE, "Expected '{'");
    BlockNode* block = (BlockNode*)create_block_node();
    while(current_token(p).type != TOKEN_RBRACE && current_token(p).type != TOKEN_EOF) {
        add_node_to_list(block->statements, parse_statement(p));
    }
    expect(p, TOKEN_RBRACE, "Expected '}' to close block");
    return (Node*)block;
}

static Node* parse_function_definition(Parser* p) {
    expect(p, TOKEN_INT, "Expected 'int' return type");
    Token name_tok = expect(p, TOKEN_IDENTIFIER, "Expected function name");
    expect(p, TOKEN_LPAREN, "Expected '(' after function name");
    // Note: We don't support parameters in this simple version
    expect(p, TOKEN_RPAREN, "Expected ')' after function parameters");
    Node* body = parse_block_statement(p);
    return create_function_def_node(strdup(name_tok.value), body);
}


// --- Public API ---

Parser* parser_create(Token* tokens, int count) {
    Parser* p = malloc(sizeof(Parser));
    p->tokens = tokens;
    p->count = count;
    p->pos = 0;
    return p;
}

void parser_destroy(Parser* parser) {
    free(parser);
}

// This is the public entry point. It parses a full program.
Node* parse(Parser* parser) {
    ProgramNode* program = (ProgramNode*)create_program_node();
    while (current_token(parser).type != TOKEN_EOF) {
        // Our simple language only supports function definitions at the top level
        add_node_to_list(program->functions, parse_function_definition(parser));
    }
    return (Node*)program;
}

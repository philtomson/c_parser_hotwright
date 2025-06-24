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
static Node* parse_while_statement(Parser* p);
static Node* parse_for_statement(Parser* p);
static Node* parse_switch_statement(Parser* p);
static Node* parse_expression_statement(Parser* p);

// Expression Parsers (for operator precedence)
static Node* parse_expression(Parser* p);
static Node* parse_comma_expression(Parser* p);
static Node* parse_assignment(Parser* p);
static Node* parse_logical_or(Parser* p);
static Node* parse_logical_and(Parser* p);
static Node* parse_bitwise_or(Parser* p);
static Node* parse_bitwise_xor(Parser* p);
static Node* parse_bitwise_and(Parser* p);
static Node* parse_equality(Parser* p);
static Node* parse_relational(Parser* p);
static Node* parse_additive(Parser* p);
static Node* parse_multiplicative(Parser* p);
static Node* parse_unary(Parser* p);
static Node* parse_postfix(Parser* p);
static Node* parse_primary(Parser* p);
static Node* parse_initializer_list(Parser* p);

static void debug_print_node(Node* node, const char* context) {
    if (!debug_mode) return;
    
    if (!node) {
        print_debug("DEBUG: %s - NULL node\n", context);
        return;
    }
    print_debug("DEBUG: %s - Node type %d at address %p\n", context, node->type, (void*)node);
    if (node->type == NODE_IDENTIFIER) {
        IdentifierNode* id = (IdentifierNode*)node;
        print_debug("  Identifier name: '%s' at address %p\n", id->name, (void*)id->name);
    }
}


// --- Helper Functions ---

static void parser_error(const char* message) {
    fprintf(stderr, "Parse Error: %s\n", message);
    exit(1);
}

static Token current_token(Parser* p) { return p->tokens[p->pos]; }

static void parser_error_at_token(Parser* p, const char* message) {
    Token token = current_token(p);
    fprintf(stderr, "Parse Error at line %d, column %d: %s\n", token.line, token.column, message);
    fprintf(stderr, "  Token: %s ('%s')\n", token_type_to_string(token.type), token.value);
    exit(1);
}
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
    fprintf(stderr, "Parse Error at line %d, column %d: %s\n", token.line, token.column, msg);
    fprintf(stderr, "  Expected: %s, but got %s ('%s')\n",
            token_type_to_string(type), token_type_to_string(token.type), token.value);
    exit(1);
}

// --- Expression Parsing Logic ---
// This chain of functions implements operator precedence.
// Each function handles one level of precedence and calls the next higher level.





static Node* parse_multiplicative(Parser* p) {
    Node* node = parse_unary(p);
    while (current_token(p).type == TOKEN_STAR || current_token(p).type == TOKEN_SLASH) {
        // Get the operator type BEFORE advancing.
        TokenType op = current_token(p).type;
        advance(p); // Manually advance instead of using match()
        Node* right = parse_unary(p);
        node = create_binary_op_node(op, node, right);
    }
    return node;
}

static Node* parse_unary(Parser* p) {
    if (current_token(p).type == TOKEN_NOT || current_token(p).type == TOKEN_MINUS) {
        TokenType op = current_token(p).type;
        advance(p);
        Node* operand = parse_unary(p); // Right-associative
        return create_unary_op_node(op, operand);
    }
    
    return parse_postfix(p);
}

static Node* parse_postfix(Parser* p) {
    Node* node = parse_primary(p);
    
    while (1) {
        if (match(p, TOKEN_LBRACKET)) {
            // Array access
            Node* index = parse_expression(p);
            expect(p, TOKEN_RBRACKET, "Expected ']' after array index");
            node = create_array_access_node(node, index);
        } else if (current_token(p).type == TOKEN_LPAREN &&
                   node->type == NODE_IDENTIFIER) {
            // Function call
            advance(p); // consume '('
            NodeList* args = create_node_list();
            
            while (current_token(p).type != TOKEN_RPAREN) {
                add_node_to_list(args, parse_expression(p));
                if (!match(p, TOKEN_COMMA)) break;
            }
            
            expect(p, TOKEN_RPAREN, "Expected ')' after function arguments");
            
            // Extract function name from identifier node
            IdentifierNode* id_node = (IdentifierNode*)node;
            node = create_function_call_node(strdup(id_node->name), args);
            
            // Free the original identifier node
            free(id_node->name);
            free(id_node);
        } else {
            break;
        }
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
    
    if (current_token(p).type == TOKEN_TRUE) {
        advance(p);
        return create_bool_literal_node(1);
    }
    
    if (current_token(p).type == TOKEN_FALSE) {
        advance(p);
        return create_bool_literal_node(0);
    }
    
    if (match(p, TOKEN_LPAREN)) {
        Node* expr = parse_expression(p);
        expect(p, TOKEN_RPAREN, "Expected ')' after expression");
        return expr;
    }

    parser_error_at_token(p, "Unexpected token in expression");
    return NULL;
}

static Node* parse_initializer_list(Parser* p) {
    expect(p, TOKEN_LBRACE, "Expected '{' to start initializer list");
    
    NodeList* elements = create_node_list();
    
    // Handle empty initializer list
    if (current_token(p).type == TOKEN_RBRACE) {
        advance(p);
        return create_initializer_list_node(elements);
    }
    
    // Parse first element
    Node* element = parse_expression(p);
    add_node_to_list(elements, element);
    
    // Parse remaining elements
    while (match(p, TOKEN_COMMA)) {
        element = parse_expression(p);
        add_node_to_list(elements, element);
    }
    
    expect(p, TOKEN_RBRACE, "Expected '}' to end initializer list");
    return create_initializer_list_node(elements);
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




static Node* parse_logical_and(Parser* p) {
    Node* node = parse_bitwise_or(p);
    while (current_token(p).type == TOKEN_LOGICAL_AND) {
        TokenType op = current_token(p).type;
        advance(p);
        Node* right = parse_bitwise_or(p);
        node = create_binary_op_node(op, node, right);
    }
    return node;
}

static Node* parse_logical_or(Parser* p) {
    Node* node = parse_logical_and(p);
    while (current_token(p).type == TOKEN_LOGICAL_OR) {
        TokenType op = current_token(p).type;
        advance(p);
        Node* right = parse_logical_and(p);
        node = create_binary_op_node(op, node, right);
    }
    return node;
}

static Node* parse_bitwise_or(Parser* p) {
    Node* node = parse_bitwise_xor(p);
    while (current_token(p).type == TOKEN_OR) {
        TokenType op = current_token(p).type;
        advance(p);
        Node* right = parse_bitwise_xor(p);
        node = create_binary_op_node(op, node, right);
    }
    return node;
}

static Node* parse_bitwise_xor(Parser* p) {
    // XOR not implemented yet, skip to bitwise AND
    return parse_bitwise_and(p);
}

static Node* parse_bitwise_and(Parser* p) {
    Node* node = parse_equality(p);
    while (current_token(p).type == TOKEN_AND) {
        TokenType op = current_token(p).type;
        advance(p);
        Node* right = parse_equality(p);
        node = create_binary_op_node(op, node, right);
    }
    return node;
}

static Node* parse_expression(Parser* p) {
    return parse_comma_expression(p);
}

static Node* parse_comma_expression(Parser* p) {
    Node* left = parse_assignment(p);
    
    while (match(p, TOKEN_COMMA)) {
        Node* right = parse_assignment(p);
        // For comma expressions, we evaluate left then right, returning right
        // We'll create a binary op node to represent this
        left = create_binary_op_node(TOKEN_COMMA, left, right);
    }
    
    return left;
}

static Node* parse_assignment(Parser* p) {
    Node* left = parse_logical_or(p);
    
    if (current_token(p).type == TOKEN_ASSIGN) {
        advance(p);
        
        if (!left) {
            parser_error("Missing left-hand side of assignment.");
        }

        if (left->type != NODE_IDENTIFIER && left->type != NODE_ARRAY_ACCESS) {
            parser_error_at_token(p, "Invalid assignment target. Must be an identifier or array element.");
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
    // Parse type (int, bool, char, unsigned char, or _BitInt)
    TokenType var_type;
    int bit_width = 0;
    int is_unsigned = 0;
    
    // Handle unsigned modifier
    if (match(p, TOKEN_UNSIGNED)) {
        is_unsigned = 1;
        // Must be followed by a type that can be unsigned
        if (match(p, TOKEN_CHAR)) {
            var_type = TOKEN_CHAR;
        } else if (match(p, TOKEN_INT)) {
            var_type = TOKEN_INT;
        } else {
            parser_error_at_token(p, "Expected 'char' or 'int' after 'unsigned'");
            return NULL;
        }
    } else if (match(p, TOKEN_CHAR)) {
        var_type = TOKEN_CHAR;
        // char defaults to signed
    } else if (match(p, TOKEN_INT)) {
        var_type = TOKEN_INT;
    } else if (match(p, TOKEN_BOOL)) {
        var_type = TOKEN_BOOL;
    } else if (match(p, TOKEN_BITINT)) {
        var_type = TOKEN_BITINT;
        // Parse _BitInt(n) syntax
        expect(p, TOKEN_LPAREN, "Expected '(' after '_BitInt'");
        Token width_tok = expect(p, TOKEN_NUMBER, "Expected bit width number");
        bit_width = atoi(width_tok.value);
        if (bit_width <= 0) {
            parser_error_at_token(p, "_BitInt width must be positive");
            return NULL;
        }
        expect(p, TOKEN_RPAREN, "Expected ')' after bit width");
    } else {
        parser_error_at_token(p, "Expected type in declaration");
        return NULL;
    }
    
    // Parse first variable
    Token id_tok = expect(p, TOKEN_IDENTIFIER, "Expected identifier in declaration");
    
    // Check for array declaration
    int array_size = 0;
    if (match(p, TOKEN_LBRACKET)) {
        Token size_tok = expect(p, TOKEN_NUMBER, "Expected array size");
        array_size = atoi(size_tok.value);
        expect(p, TOKEN_RBRACKET, "Expected ']' after array size");
    }
    
    Node* initializer = NULL;
    if (match(p, TOKEN_ASSIGN)) {
        // Check if this is an array with initializer list or _BitInt with bit initializer
        if ((array_size > 0 || var_type == TOKEN_BITINT) && current_token(p).type == TOKEN_LBRACE) {
            initializer = parse_initializer_list(p);
        } else {
            initializer = parse_expression(p);
        }
    }
    
    // Create first variable declaration
    Node* first_decl = create_var_decl_node(var_type, is_unsigned, strdup(id_tok.value), array_size, bit_width, initializer);
    
    // Check for comma-separated additional variables
    if (current_token(p).type == TOKEN_COMMA) {
        // Create a block to hold multiple declarations
        BlockNode* block = (BlockNode*)create_block_node();
        add_node_to_list(block->statements, first_decl);
        
        while (match(p, TOKEN_COMMA)) {
            Token next_id_tok = expect(p, TOKEN_IDENTIFIER, "Expected identifier after ','");
            
            // Check for array declaration on additional variables
            int next_array_size = 0;
            if (match(p, TOKEN_LBRACKET)) {
                Token size_tok = expect(p, TOKEN_NUMBER, "Expected array size");
                next_array_size = atoi(size_tok.value);
                expect(p, TOKEN_RBRACKET, "Expected ']' after array size");
            }
            
            Node* next_initializer = NULL;
            if (match(p, TOKEN_ASSIGN)) {
                // Check if this is an array with initializer list or _BitInt with bit initializer
                if ((next_array_size > 0 || var_type == TOKEN_BITINT) && current_token(p).type == TOKEN_LBRACE) {
                    next_initializer = parse_initializer_list(p);
                } else {
                    next_initializer = parse_expression(p);
                }
            }
            
            Node* next_decl = create_var_decl_node(var_type, is_unsigned, strdup(next_id_tok.value), next_array_size, bit_width, next_initializer);
            add_node_to_list(block->statements, next_decl);
        }
        
        expect(p, TOKEN_SEMICOLON, "Expected ';' after declaration");
        return (Node*)block;
    } else {
        // Single variable declaration
        expect(p, TOKEN_SEMICOLON, "Expected ';' after declaration");
        return first_decl;
    }
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

static Node* parse_while_statement(Parser* p) {
    expect(p, TOKEN_WHILE, "Expected 'while'");
    expect(p, TOKEN_LPAREN, "Expected '(' after 'while'");
    Node* condition = parse_expression(p);
    expect(p, TOKEN_RPAREN, "Expected ')' after while condition");
    
    Node* body = parse_statement(p);
    
    return create_while_node(condition, body);
}

static Node* parse_for_statement(Parser* p) {
    expect(p, TOKEN_FOR, "Expected 'for'");
    expect(p, TOKEN_LPAREN, "Expected '(' after 'for'");
    
    // Parse init (can be empty)
    Node* init = NULL;
    if (current_token(p).type != TOKEN_SEMICOLON) {
        if ((current_token(p).type == TOKEN_INT || current_token(p).type == TOKEN_BOOL) &&
            peek_token(p).type == TOKEN_IDENTIFIER) {
            init = parse_declaration_statement(p);
            // Declaration already consumes semicolon
        } else {
            init = parse_expression(p);
            expect(p, TOKEN_SEMICOLON, "Expected ';' after for loop initializer");
        }
    } else {
        advance(p); // consume semicolon
    }
    
    // Parse condition (can be empty)
    Node* condition = NULL;
    if (current_token(p).type != TOKEN_SEMICOLON) {
        condition = parse_expression(p);
    }
    expect(p, TOKEN_SEMICOLON, "Expected ';' after for loop condition");
    
    // Parse update (can be empty)
    Node* update = NULL;
    if (current_token(p).type != TOKEN_RPAREN) {
        update = parse_expression(p);
    }
    expect(p, TOKEN_RPAREN, "Expected ')' after for loop clauses");
    
    Node* body = parse_statement(p);
    
    return create_for_node(init, condition, update, body);
}

static Node* parse_statement(Parser* p) {
    // Check for declaration: 'int', 'bool', 'char', 'unsigned', or '_BitInt' followed by an identifier
    if ((current_token(p).type == TOKEN_INT || current_token(p).type == TOKEN_BOOL ||
         current_token(p).type == TOKEN_CHAR || current_token(p).type == TOKEN_UNSIGNED ||
         current_token(p).type == TOKEN_BITINT)) {
        // For _BitInt, we need to check if it's followed by (n) and then identifier
        if (current_token(p).type == TOKEN_BITINT) {
            return parse_declaration_statement(p);
        } else if (current_token(p).type == TOKEN_UNSIGNED) {
            // unsigned must be followed by a type (char or int) and then identifier
            return parse_declaration_statement(p);
        } else if (peek_token(p).type == TOKEN_IDENTIFIER) {
            return parse_declaration_statement(p);
        }
    }

    switch (current_token(p).type) {
        case TOKEN_IF: return parse_if_statement(p);
        case TOKEN_WHILE: return parse_while_statement(p);
        case TOKEN_FOR: return parse_for_statement(p);
        case TOKEN_SWITCH: return parse_switch_statement(p);
        case TOKEN_BREAK:
            advance(p);
            expect(p, TOKEN_SEMICOLON, "Expected ';' after break");
            return create_break_node();
        case TOKEN_CONTINUE:
            advance(p);
            expect(p, TOKEN_SEMICOLON, "Expected ';' after continue");
            return create_continue_node();
        case TOKEN_RETURN:
            advance(p);
            Node* return_value = NULL;
            if (current_token(p).type != TOKEN_SEMICOLON) {
                return_value = parse_expression(p);
            }
            expect(p, TOKEN_SEMICOLON, "Expected ';' after return");
            return create_return_node(return_value);
        case TOKEN_LBRACE: return parse_block_statement(p);
        
        // Add other statement types like function calls here
        
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
    // Accept int, char, unsigned char, and void return types
    if (current_token(p).type == TOKEN_INT) {
        advance(p);
    } else if (current_token(p).type == TOKEN_CHAR) {
        advance(p);
    } else if (current_token(p).type == TOKEN_UNSIGNED) {
        advance(p);
        expect(p, TOKEN_CHAR, "Expected 'char' after 'unsigned'");
    } else if (current_token(p).type == TOKEN_VOID) {
        advance(p);
    } else {
        parser_error_at_token(p, "Expected return type (int, char, unsigned char, or void)");
        return NULL;
    }
    Token name_tok = expect(p, TOKEN_IDENTIFIER, "Expected function name");
    expect(p, TOKEN_LPAREN, "Expected '(' after function name");
    
    NodeList* parameters = create_node_list();
    
    // Parse parameters
    if (current_token(p).type != TOKEN_RPAREN) {
        // First parameter
        if (current_token(p).type == TOKEN_INT) {
            advance(p);
        } else if (current_token(p).type == TOKEN_CHAR) {
            advance(p);
        } else if (current_token(p).type == TOKEN_UNSIGNED) {
            advance(p);
            expect(p, TOKEN_CHAR, "Expected 'char' after 'unsigned'");
        } else {
            parser_error_at_token(p, "Expected parameter type (int, char, or unsigned char)");
            return NULL;
        }
        Token param_tok = expect(p, TOKEN_IDENTIFIER, "Expected parameter name");
        add_node_to_list(parameters, create_identifier_node(strdup(param_tok.value)));
        
        // Additional parameters
        while (match(p, TOKEN_COMMA)) {
            if (current_token(p).type == TOKEN_INT) {
                advance(p);
            } else if (current_token(p).type == TOKEN_CHAR) {
                advance(p);
            } else if (current_token(p).type == TOKEN_UNSIGNED) {
                advance(p);
                expect(p, TOKEN_CHAR, "Expected 'char' after 'unsigned'");
            } else {
                parser_error_at_token(p, "Expected parameter type (int, char, or unsigned char)");
                return NULL;
            }
            param_tok = expect(p, TOKEN_IDENTIFIER, "Expected parameter name");
            add_node_to_list(parameters, create_identifier_node(strdup(param_tok.value)));
        }
    }
    
    expect(p, TOKEN_RPAREN, "Expected ')' after function parameters");
    Node* body = parse_block_statement(p);
    return create_function_def_node(strdup(name_tok.value), parameters, body);
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
        // Check if this is a function definition or global variable declaration
        if (current_token(parser).type == TOKEN_INT || current_token(parser).type == TOKEN_BOOL ||
            current_token(parser).type == TOKEN_CHAR || current_token(parser).type == TOKEN_UNSIGNED ||
            current_token(parser).type == TOKEN_VOID || current_token(parser).type == TOKEN_BITINT) {
            
            // Look ahead to see if this is a function (has parentheses) or variable
            Parser temp_parser = *parser;
            advance(&temp_parser); // skip type
            
            // For _BitInt, we need to skip the (n) part
            if (current_token(parser).type == TOKEN_BITINT) {
                if (current_token(&temp_parser).type == TOKEN_LPAREN) {
                    advance(&temp_parser); // skip '('
                    advance(&temp_parser); // skip number
                    advance(&temp_parser); // skip ')'
                }
            }
            
            // For unsigned, we need to skip the following type (char or int)
            if (current_token(parser).type == TOKEN_UNSIGNED) {
                if (current_token(&temp_parser).type == TOKEN_CHAR || current_token(&temp_parser).type == TOKEN_INT) {
                    advance(&temp_parser); // skip the type after unsigned
                }
            }
            
            // Now check for identifier
            if (current_token(&temp_parser).type == TOKEN_IDENTIFIER) {
                advance(&temp_parser); // skip identifier
                
                if (current_token(&temp_parser).type == TOKEN_LPAREN) {
                    // This is a function definition
                    add_node_to_list(program->functions, parse_function_definition(parser));
                } else {
                    // This is a global variable declaration
                    add_node_to_list(program->functions, parse_declaration_statement(parser));
                }
            } else {
                parser_error("Expected identifier after type");
            }
        } else {
            Token token = current_token(parser);
            char error_msg[512];
            
            if (token.value && token.value[0] == '#') {
                snprintf(error_msg, sizeof(error_msg),
                    "Preprocessor directives like '%s' are not supported. "
                    "Please use standard C declarations instead.", token.value);
            } else if (token.value && strcmp(token.value, "continue") == 0) {
                snprintf(error_msg, sizeof(error_msg),
                    "'continue' statements are not supported. Use 'break' instead.");
            } else if (token.value && strcmp(token.value, "_Bool") == 0) {
                snprintf(error_msg, sizeof(error_msg),
                    "Type '_Bool' is not directly supported. Use 'bool' instead, or declare as 'int'.");
            } else {
                snprintf(error_msg, sizeof(error_msg),
                    "Expected function definition or global variable declaration. "
                    "Got '%s' (%s). Supported types: int, bool, char, unsigned char, void, _BitInt(n). "
                    "Common issues: preprocessor directives (#define), "
                    "comma operator in expressions, continue statements.",
                    token.value ? token.value : "unknown", token_type_to_string(token.type));
            }
            
            parser_error_at_token(parser, error_msg);
        }
    }
    return (Node*)program;
}

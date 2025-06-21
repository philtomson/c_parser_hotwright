#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>

typedef enum {
    // Keywords
    TOKEN_INT, TOKEN_BOOL, TOKEN_BITINT, TOKEN_TRUE, TOKEN_FALSE, TOKEN_IF, TOKEN_ELSE, TOKEN_WHILE, TOKEN_FOR, TOKEN_RETURN, TOKEN_BREAK,
    TOKEN_SWITCH, TOKEN_CASE, TOKEN_DEFAULT,
    // Preprocessor directives
    TOKEN_INCLUDE,
    // Identifiers and Literals
    TOKEN_IDENTIFIER, TOKEN_NUMBER, TOKEN_STRING,
    // Operators
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH,
    TOKEN_ASSIGN, TOKEN_EQUAL, TOKEN_NOT_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL, TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_AND, TOKEN_OR, TOKEN_LOGICAL_AND, TOKEN_LOGICAL_OR, TOKEN_NOT,
    // Punctuation
    TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE, TOKEN_LBRACKET, TOKEN_RBRACKET,
    TOKEN_SEMICOLON, TOKEN_COLON, TOKEN_COMMA,
    // End of File, Illegal
    TOKEN_EOF, TOKEN_ILLEGAL
} TokenType;

typedef struct {
    TokenType type;
    char* value; // Malloc'd string
    int line;    // Line number where token appears
    int column;  // Column number where token starts
} Token;

// Opaque struct for the lexer state
typedef struct Lexer Lexer;

Lexer* lexer_create(const char* source);
Token lexer_next_token(Lexer* lexer);
void lexer_destroy(Lexer* lexer);
const char* token_type_to_string(TokenType type); // Helper for printing

#endif // LEXER_H

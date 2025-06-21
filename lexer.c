#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct Lexer {
    const char* source;
    int pos;
    int len;
    int line;
    int column;
};

Lexer* lexer_create(const char* source) {
    Lexer* lexer = malloc(sizeof(Lexer));
    lexer->source = source;
    lexer->pos = 0;
    lexer->len = strlen(source);
    lexer->line = 1;
    lexer->column = 1;
    return lexer;
}

void lexer_destroy(Lexer* lexer) {
    free(lexer);
}

// Helper to create a token and copy its value
static Token make_token(TokenType type, const char* value, int len, int line, int column) {
    Token token;
    token.type = type;
    token.value = malloc(len + 1);
    strncpy(token.value, value, len);
    token.value[len] = '\0';
    token.line = line;
    token.column = column;
    return token;
}

// Helper for single-char tokens
static Token make_simple_token(TokenType type, Lexer* lexer) {
    Token token = make_token(type, &lexer->source[lexer->pos], 1, lexer->line, lexer->column);
    lexer->pos++;
    lexer->column++;
    return token;
}

static void skip_whitespace_and_comments(Lexer* lexer) {
    while (lexer->pos < lexer->len) {
        // Skip whitespace
        if (isspace(lexer->source[lexer->pos])) {
            if (lexer->source[lexer->pos] == '\n') {
                lexer->line++;
                lexer->column = 1;
            } else {
                lexer->column++;
            }
            lexer->pos++;
            continue;
        }
        
        // Skip single-line comments
        if (lexer->pos + 1 < lexer->len &&
            lexer->source[lexer->pos] == '/' &&
            lexer->source[lexer->pos + 1] == '/') {
            lexer->pos += 2;
            lexer->column += 2;
            while (lexer->pos < lexer->len && lexer->source[lexer->pos] != '\n') {
                lexer->pos++;
                lexer->column++;
            }
            continue;
        }
        
        // Skip multi-line comments
        if (lexer->pos + 1 < lexer->len &&
            lexer->source[lexer->pos] == '/' &&
            lexer->source[lexer->pos + 1] == '*') {
            lexer->pos += 2;
            lexer->column += 2;
            while (lexer->pos + 1 < lexer->len) {
                if (lexer->source[lexer->pos] == '*' &&
                    lexer->source[lexer->pos + 1] == '/') {
                    lexer->pos += 2;
                    lexer->column += 2;
                    break;
                }
                if (lexer->source[lexer->pos] == '\n') {
                    lexer->line++;
                    lexer->column = 1;
                } else {
                    lexer->column++;
                }
                lexer->pos++;
            }
            continue;
        }
        
        // No more whitespace or comments
        break;
    }
}

static Token identifier_or_keyword(Lexer* lexer) {
    int start = lexer->pos;
    int start_column = lexer->column;
    while (lexer->pos < lexer->len && (isalnum(lexer->source[lexer->pos]) || lexer->source[lexer->pos] == '_')) {
        lexer->pos++;
        lexer->column++;
    }
    int len = lexer->pos - start;
    char* value = (char*)malloc(len + 1);
    strncpy(value, &lexer->source[start], len);
    value[len] = '\0';

    // Keyword check
    if (strcmp(value, "int") == 0) { free(value); return make_token(TOKEN_INT, "int", 3, lexer->line, start_column); }
    if (strcmp(value, "bool") == 0) { free(value); return make_token(TOKEN_BOOL, "bool", 4, lexer->line, start_column); }
    if (strcmp(value, "_BitInt") == 0) { free(value); return make_token(TOKEN_BITINT, "_BitInt", 7, lexer->line, start_column); }
    if (strcmp(value, "true") == 0) { free(value); return make_token(TOKEN_TRUE, "true", 4, lexer->line, start_column); }
    if (strcmp(value, "false") == 0) { free(value); return make_token(TOKEN_FALSE, "false", 5, lexer->line, start_column); }
    if (strcmp(value, "if") == 0) { free(value); return make_token(TOKEN_IF, "if", 2, lexer->line, start_column); }
    if (strcmp(value, "else") == 0) { free(value); return make_token(TOKEN_ELSE, "else", 4, lexer->line, start_column); }
    if (strcmp(value, "while") == 0) { free(value); return make_token(TOKEN_WHILE, "while", 5, lexer->line, start_column); }
    if (strcmp(value, "for") == 0) { free(value); return make_token(TOKEN_FOR, "for", 3, lexer->line, start_column); }
    if (strcmp(value, "return") == 0) { free(value); return make_token(TOKEN_RETURN, "return", 6, lexer->line, start_column); }
    if (strcmp(value, "break") == 0) { free(value); return make_token(TOKEN_BREAK, "break", 5, lexer->line, start_column); }
    if (strcmp(value, "switch") == 0) { free(value); return make_token(TOKEN_SWITCH, "switch", 6, lexer->line, start_column); }
    if (strcmp(value, "case") == 0) { free(value); return make_token(TOKEN_CASE, "case", 4, lexer->line, start_column); }
    if (strcmp(value, "default") == 0) { free(value); return make_token(TOKEN_DEFAULT, "default", 7, lexer->line, start_column); }
    
    Token token = make_token(TOKEN_IDENTIFIER, value, strlen(value), lexer->line, start_column);
    free(value);  // Free the temporary value since make_token makes its own copy
    return token;
}

static Token number(Lexer* lexer) {
    int start = lexer->pos;
    int start_column = lexer->column;
    while (lexer->pos < lexer->len && isdigit(lexer->source[lexer->pos])) {
        lexer->pos++;
        lexer->column++;
    }
    return make_token(TOKEN_NUMBER, &lexer->source[start], lexer->pos - start, lexer->line, start_column);
}

static Token string_literal(Lexer* lexer) {
    int start = lexer->pos;
    int start_column = lexer->column;
    lexer->pos++; // Skip opening quote
    lexer->column++;
    
    while (lexer->pos < lexer->len && lexer->source[lexer->pos] != '"') {
        if (lexer->source[lexer->pos] == '\n') {
            lexer->line++;
            lexer->column = 1;
        } else {
            lexer->column++;
        }
        lexer->pos++;
    }
    
    if (lexer->pos >= lexer->len) {
        // Unterminated string
        return make_token(TOKEN_ILLEGAL, &lexer->source[start], lexer->pos - start, lexer->line, start_column);
    }
    
    lexer->pos++; // Skip closing quote
    lexer->column++;
    
    // Return string without quotes
    return make_token(TOKEN_STRING, &lexer->source[start + 1], lexer->pos - start - 2, lexer->line, start_column);
}

static Token handle_include_directive(Lexer* lexer) {
    int start_column = lexer->column;
    lexer->pos++; // Skip '#'
    lexer->column++;
    
    // Skip whitespace after #
    while (lexer->pos < lexer->len && (lexer->source[lexer->pos] == ' ' || lexer->source[lexer->pos] == '\t')) {
        lexer->pos++;
        lexer->column++;
    }
    
    // Check if it's "include"
    if (lexer->pos + 7 <= lexer->len && strncmp(&lexer->source[lexer->pos], "include", 7) == 0) {
        lexer->pos += 7;
        lexer->column += 7;
        return make_token(TOKEN_INCLUDE, "#include", 8, lexer->line, start_column);
    }
    
    // Not an include directive, treat as illegal
    return make_simple_token(TOKEN_ILLEGAL, lexer);
}

Token lexer_next_token(Lexer* lexer) {
    skip_whitespace_and_comments(lexer);

    if (lexer->pos >= lexer->len) return make_token(TOKEN_EOF, "", 0, lexer->line, lexer->column);

    char current = lexer->source[lexer->pos];
    char peek = (lexer->pos + 1 < lexer->len) ? lexer->source[lexer->pos + 1] : '\0';

    if (current == '#') return handle_include_directive(lexer);
    if (current == '"') return string_literal(lexer);
    if (isalpha(current) || current == '_') return identifier_or_keyword(lexer);
    if (isdigit(current)) return number(lexer);

    switch (current) {
        case '+': return make_simple_token(TOKEN_PLUS, lexer);
        case '-': return make_simple_token(TOKEN_MINUS, lexer);
        case '*': return make_simple_token(TOKEN_STAR, lexer);
        case '/': return make_simple_token(TOKEN_SLASH, lexer);
        case '(': return make_simple_token(TOKEN_LPAREN, lexer);
        case ')': return make_simple_token(TOKEN_RPAREN, lexer);
        case '{': return make_simple_token(TOKEN_LBRACE, lexer);
        case '}': return make_simple_token(TOKEN_RBRACE, lexer);
        case '[': return make_simple_token(TOKEN_LBRACKET, lexer);
        case ']': return make_simple_token(TOKEN_RBRACKET, lexer);
        case ';': return make_simple_token(TOKEN_SEMICOLON, lexer);
        case ':': return make_simple_token(TOKEN_COLON, lexer);
        case ',': return make_simple_token(TOKEN_COMMA, lexer);
        case '=':
            if (peek == '=') {
                int col = lexer->column;
                lexer->pos += 2;
                lexer->column += 2;
                return make_token(TOKEN_EQUAL, "==", 2, lexer->line, col);
            }
            return make_simple_token(TOKEN_ASSIGN, lexer);
        case '!':
            if (peek == '=') {
                int col = lexer->column;
                lexer->pos += 2;
                lexer->column += 2;
                return make_token(TOKEN_NOT_EQUAL, "!=", 2, lexer->line, col);
            }
            return make_simple_token(TOKEN_NOT, lexer);
        case '&':
            if (peek == '&') {
                int col = lexer->column;
                lexer->pos += 2;
                lexer->column += 2;
                return make_token(TOKEN_LOGICAL_AND, "&&", 2, lexer->line, col);
            }
            return make_simple_token(TOKEN_AND, lexer);
        case '|':
            if (peek == '|') {
                int col = lexer->column;
                lexer->pos += 2;
                lexer->column += 2;
                return make_token(TOKEN_LOGICAL_OR, "||", 2, lexer->line, col);
            }
            return make_simple_token(TOKEN_OR, lexer);
        case '<':
            if (peek == '=') {
                int col = lexer->column;
                lexer->pos += 2;
                lexer->column += 2;
                return make_token(TOKEN_LESS_EQUAL, "<=", 2, lexer->line, col);
            }
            return make_simple_token(TOKEN_LESS, lexer);
        case '>':
            if (peek == '=') {
                int col = lexer->column;
                lexer->pos += 2;
                lexer->column += 2;
                return make_token(TOKEN_GREATER_EQUAL, ">=", 2, lexer->line, col);
            }
            return make_simple_token(TOKEN_GREATER, lexer);
    }
    
    return make_simple_token(TOKEN_ILLEGAL, lexer);
}

// For printing/debugging
const char* token_type_to_string(TokenType type) {
    switch(type) {
        case TOKEN_INT: return "INT"; case TOKEN_BOOL: return "BOOL"; case TOKEN_BITINT: return "BITINT"; case TOKEN_TRUE: return "TRUE"; case TOKEN_FALSE: return "FALSE";
        case TOKEN_IF: return "IF"; case TOKEN_ELSE: return "ELSE";
        case TOKEN_WHILE: return "WHILE"; case TOKEN_FOR: return "FOR"; case TOKEN_RETURN: return "RETURN"; case TOKEN_BREAK: return "BREAK";
        case TOKEN_SWITCH: return "SWITCH"; case TOKEN_CASE: return "CASE"; case TOKEN_DEFAULT: return "DEFAULT";
        case TOKEN_INCLUDE: return "INCLUDE";
        case TOKEN_IDENTIFIER: return "IDENTIFIER"; case TOKEN_NUMBER: return "NUMBER"; case TOKEN_STRING: return "STRING";
        case TOKEN_PLUS: return "PLUS"; case TOKEN_MINUS: return "MINUS"; case TOKEN_STAR: return "STAR";
        case TOKEN_SLASH: return "SLASH"; case TOKEN_ASSIGN: return "ASSIGN"; case TOKEN_EQUAL: return "EQUAL";
        case TOKEN_NOT_EQUAL: return "NOT_EQUAL"; case TOKEN_LESS: return "LESS"; case TOKEN_LESS_EQUAL: return "LESS_EQUAL";
        case TOKEN_GREATER: return "GREATER"; case TOKEN_GREATER_EQUAL: return "GREATER_EQUAL";
        case TOKEN_AND: return "AND"; case TOKEN_OR: return "OR"; case TOKEN_LOGICAL_AND: return "LOGICAL_AND";
        case TOKEN_LOGICAL_OR: return "LOGICAL_OR"; case TOKEN_NOT: return "NOT";
        case TOKEN_LPAREN: return "LPAREN"; case TOKEN_RPAREN: return "RPAREN"; case TOKEN_LBRACE: return "LBRACE";
        case TOKEN_RBRACE: return "RBRACE"; case TOKEN_LBRACKET: return "LBRACKET"; case TOKEN_RBRACKET: return "RBRACKET";
        case TOKEN_SEMICOLON: return "SEMICOLON"; case TOKEN_COLON: return "COLON";
        case TOKEN_COMMA: return "COMMA"; case TOKEN_EOF: return "EOF"; case TOKEN_ILLEGAL: return "ILLEGAL";
    }
    return "UNKNOWN";
}

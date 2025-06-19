#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct Lexer {
    const char* source;
    int pos;
    int len;
};

Lexer* lexer_create(const char* source) {
    Lexer* lexer = malloc(sizeof(Lexer));
    lexer->source = source;
    lexer->pos = 0;
    lexer->len = strlen(source);
    return lexer;
}

void lexer_destroy(Lexer* lexer) {
    free(lexer);
}

// Helper to create a token and copy its value
static Token make_token(TokenType type, const char* value, int len) {
    Token token;
    token.type = type;
    token.value = malloc(len + 1);
    strncpy(token.value, value, len);
    token.value[len] = '\0';
    return token;
}

// Helper for single-char tokens
static Token make_simple_token(TokenType type, Lexer* lexer) {
    Token token = make_token(type, &lexer->source[lexer->pos], 1);
    lexer->pos++;
    return token;
}

static void skip_whitespace(Lexer* lexer) {
    while (lexer->pos < lexer->len && isspace(lexer->source[lexer->pos])) {
        lexer->pos++;
    }
}

static Token identifier_or_keyword(Lexer* lexer) {
    int start = lexer->pos;
    while (lexer->pos < lexer->len && (isalnum(lexer->source[lexer->pos]) || lexer->source[lexer->pos] == '_')) {
        lexer->pos++;
    }
    int len = lexer->pos - start;
    char* value = (char*)malloc(len + 1);
    strncpy(value, &lexer->source[start], len);
    value[len] = '\0';

    // Keyword check
    if (strcmp(value, "int") == 0) { free(value); return make_token(TOKEN_INT, "int", 3); }
    if (strcmp(value, "if") == 0) { free(value); return make_token(TOKEN_IF, "if", 2); }
    if (strcmp(value, "else") == 0) { free(value); return make_token(TOKEN_ELSE, "else", 4); }
    if (strcmp(value, "while") == 0) { free(value); return make_token(TOKEN_WHILE, "while", 5); }
    if (strcmp(value, "return") == 0) { free(value); return make_token(TOKEN_RETURN, "return", 6); }
    if (strcmp(value, "break") == 0) { free(value); return make_token(TOKEN_BREAK, "break", 5); }
    if (strcmp(value, "switch") == 0) { free(value); return make_token(TOKEN_SWITCH, "switch", 6); }
    if (strcmp(value, "case") == 0) { free(value); return make_token(TOKEN_CASE, "case", 4); }
    if (strcmp(value, "default") == 0) { free(value); return make_token(TOKEN_DEFAULT, "default", 7); }
    
    Token token = make_token(TOKEN_IDENTIFIER, value, strlen(value));
    free(value);  // Free the temporary value since make_token makes its own copy
    return token;
}

static Token number(Lexer* lexer) {
    int start = lexer->pos;
    while (lexer->pos < lexer->len && isdigit(lexer->source[lexer->pos])) {
        lexer->pos++;
    }
    return make_token(TOKEN_NUMBER, &lexer->source[start], lexer->pos - start);
}

Token lexer_next_token(Lexer* lexer) {
    skip_whitespace(lexer);

    if (lexer->pos >= lexer->len) return make_token(TOKEN_EOF, "", 0);

    char current = lexer->source[lexer->pos];
    char peek = (lexer->pos + 1 < lexer->len) ? lexer->source[lexer->pos + 1] : '\0';

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
        case ';': return make_simple_token(TOKEN_SEMICOLON, lexer);
        case ':': return make_simple_token(TOKEN_COLON, lexer);
        case '=':
            if (peek == '=') { lexer->pos += 2; return make_token(TOKEN_EQUAL, "==", 2); }
            return make_simple_token(TOKEN_ASSIGN, lexer);
        case '!':
            if (peek == '=') { lexer->pos += 2; return make_token(TOKEN_NOT_EQUAL, "!=", 2); }
            break;
        case '<':
            if (peek == '=') { lexer->pos += 2; return make_token(TOKEN_LESS_EQUAL, "<=", 2); }
            return make_simple_token(TOKEN_LESS, lexer);
        case '>':
            if (peek == '=') { lexer->pos += 2; return make_token(TOKEN_GREATER_EQUAL, ">=", 2); }
            return make_simple_token(TOKEN_GREATER, lexer);
    }
    
    return make_simple_token(TOKEN_ILLEGAL, lexer);
}

// For printing/debugging
const char* token_type_to_string(TokenType type) {
    switch(type) {
        case TOKEN_INT: return "INT"; case TOKEN_IF: return "IF"; case TOKEN_ELSE: return "ELSE";
        case TOKEN_WHILE: return "WHILE"; case TOKEN_RETURN: return "RETURN"; case TOKEN_BREAK: return "BREAK";
        case TOKEN_SWITCH: return "SWITCH"; case TOKEN_CASE: return "CASE"; case TOKEN_DEFAULT: return "DEFAULT";
        case TOKEN_IDENTIFIER: return "IDENTIFIER"; case TOKEN_NUMBER: return "NUMBER";
        case TOKEN_PLUS: return "PLUS"; case TOKEN_MINUS: return "MINUS"; case TOKEN_STAR: return "STAR";
        case TOKEN_SLASH: return "SLASH"; case TOKEN_ASSIGN: return "ASSIGN"; case TOKEN_EQUAL: return "EQUAL";
        case TOKEN_NOT_EQUAL: return "NOT_EQUAL"; case TOKEN_LESS: return "LESS"; case TOKEN_LESS_EQUAL: return "LESS_EQUAL";
        case TOKEN_GREATER: return "GREATER"; case TOKEN_GREATER_EQUAL: return "GREATER_EQUAL";
        case TOKEN_LPAREN: return "LPAREN"; case TOKEN_RPAREN: return "RPAREN"; case TOKEN_LBRACE: return "LBRACE";
        case TOKEN_RBRACE: return "RBRACE"; case TOKEN_SEMICOLON: return "SEMICOLON"; case TOKEN_COLON: return "COLON";
        case TOKEN_EOF: return "EOF"; case TOKEN_ILLEGAL: return "ILLEGAL";
    }
    return "UNKNOWN";
}

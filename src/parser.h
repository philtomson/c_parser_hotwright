#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct {
    Token* tokens;
    int pos;
    int count;
} Parser;

Parser* parser_create(Token* tokens, int count);
void parser_destroy(Parser* parser);

// The main entry point
Node* parse(Parser* parser);

#endif // PARSER_H

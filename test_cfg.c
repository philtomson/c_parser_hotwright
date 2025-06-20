#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include "cfg.h"
#include "cfg_builder.h"
#include "cfg_utils.h"

// Global debug flag for test executable
int debug_mode = 0;

void test_simple_function() {
    const char* code =
        "int main() {\n"
        "    int x = 5;\n"
        "    int y = 10;\n"
        "    int z = x + y;\n"
        "    return z;\n"
        "}\n";
    
    printf("=== Testing Simple Function ===\n");
    printf("Source code:\n%s\n", code);
    
    // Tokenize
    Lexer* lexer = lexer_create(code);
    Token tokens[1024];
    int token_count = 0;
    Token token;
    do {
        token = lexer_next_token(lexer);
        tokens[token_count++] = token;
    } while (token.type != TOKEN_EOF && token_count < 1024);
    lexer_destroy(lexer);
    
    // Parse the code
    Parser* parser = parser_create(tokens, token_count);
    Node* ast = parse(parser);
    
    if (!ast) {
        printf("Failed to parse program\n");
        parser_destroy(parser);
        for (int i = 0; i < token_count; i++) {
            free(tokens[i].value);
        }
        return;
    }
    
    // Build CFG
    CFG* cfg = build_cfg_from_ast(ast);
    if (!cfg) {
        printf("Failed to build CFG\n");
        free_node(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        return;
    }
    
    // Print CFG
    print_cfg(cfg);
    
    // Generate DOT file
    cfg_to_dot(cfg, "simple_function.dot");
    printf("\nGenerated DOT file: simple_function.dot\n");
    printf("To visualize: dot -Tpng simple_function.dot -o simple_function.png\n");
    
    // Verify CFG
    verify_cfg(cfg);
    
    // Cleanup
    free_cfg(cfg);
    free_node(ast);
    parser_destroy(parser);
    for (int i = 0; i < token_count; i++) {
        free(tokens[i].value);
    }
}

void test_if_statement() {
    const char* code =
        "int max(int a, int b) {\n"
        "    if (a > b) {\n"
        "        return a;\n"
        "    } else {\n"
        "        return b;\n"
        "    }\n"
        "}\n";
    
    printf("\n=== Testing If Statement ===\n");
    printf("Source code:\n%s\n", code);
    
    // Tokenize
    Lexer* lexer = lexer_create(code);
    Token tokens[1024];
    int token_count = 0;
    Token token;
    do {
        token = lexer_next_token(lexer);
        tokens[token_count++] = token;
    } while (token.type != TOKEN_EOF && token_count < 1024);
    lexer_destroy(lexer);
    
    // Parse the code
    Parser* parser = parser_create(tokens, token_count);
    Node* ast = parse(parser);
    
    if (!ast) {
        printf("Failed to parse program\n");
        parser_destroy(parser);
        for (int i = 0; i < token_count; i++) {
            free(tokens[i].value);
        }
        return;
    }
    
    // Build CFG
    CFG* cfg = build_cfg_from_ast(ast);
    if (!cfg) {
        printf("Failed to build CFG\n");
        free_node(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        return;
    }
    
    // Print CFG
    print_cfg(cfg);
    
    // Generate DOT file
    cfg_to_dot(cfg, "if_statement.dot");
    printf("\nGenerated DOT file: if_statement.dot\n");
    
    // Cleanup
    free_cfg(cfg);
    free_node(ast);
    parser_destroy(parser);
    for (int i = 0; i < token_count; i++) {
        free(tokens[i].value);
    }
}

void test_while_loop() {
    const char* code =
        "int sum(int n) {\n"
        "    int total = 0;\n"
        "    int i = 0;\n"
        "    while (i < n) {\n"
        "        total = total + i;\n"
        "        i = i + 1;\n"
        "    }\n"
        "    return total;\n"
        "}\n";
    
    printf("\n=== Testing While Loop ===\n");
    printf("Source code:\n%s\n", code);
    
    // Tokenize
    Lexer* lexer = lexer_create(code);
    Token tokens[1024];
    int token_count = 0;
    Token token;
    do {
        token = lexer_next_token(lexer);
        tokens[token_count++] = token;
    } while (token.type != TOKEN_EOF && token_count < 1024);
    lexer_destroy(lexer);
    
    // Parse the code
    Parser* parser = parser_create(tokens, token_count);
    Node* ast = parse(parser);
    
    if (!ast) {
        printf("Failed to parse program\n");
        parser_destroy(parser);
        for (int i = 0; i < token_count; i++) {
            free(tokens[i].value);
        }
        return;
    }
    
    // Build CFG
    CFG* cfg = build_cfg_from_ast(ast);
    if (!cfg) {
        printf("Failed to build CFG\n");
        free_node(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        return;
    }
    
    // Print CFG
    print_cfg(cfg);
    
    // Generate DOT file
    cfg_to_dot(cfg, "while_loop.dot");
    printf("\nGenerated DOT file: while_loop.dot\n");
    
    // Cleanup
    free_cfg(cfg);
    free_node(ast);
    parser_destroy(parser);
    for (int i = 0; i < token_count; i++) {
        free(tokens[i].value);
    }
}

void test_for_loop() {
    const char* code =
        "int factorial(int n) {\n"
        "    int result = 1;\n"
        "    for (int i = 1; i <= n; i = i + 1) {\n"
        "        result = result * i;\n"
        "    }\n"
        "    return result;\n"
        "}\n";
    
    printf("\n=== Testing For Loop ===\n");
    printf("Source code:\n%s\n", code);
    
    // Tokenize
    Lexer* lexer = lexer_create(code);
    Token tokens[1024];
    int token_count = 0;
    Token token;
    do {
        token = lexer_next_token(lexer);
        tokens[token_count++] = token;
    } while (token.type != TOKEN_EOF && token_count < 1024);
    lexer_destroy(lexer);
    
    // Parse the code
    Parser* parser = parser_create(tokens, token_count);
    Node* ast = parse(parser);
    
    if (!ast) {
        printf("Failed to parse program\n");
        parser_destroy(parser);
        for (int i = 0; i < token_count; i++) {
            free(tokens[i].value);
        }
        return;
    }
    
    // Build CFG
    CFG* cfg = build_cfg_from_ast(ast);
    if (!cfg) {
        printf("Failed to build CFG\n");
        free_node(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        return;
    }
    
    // Print CFG
    print_cfg(cfg);
    
    // Generate DOT file
    cfg_to_dot(cfg, "for_loop.dot");
    printf("\nGenerated DOT file: for_loop.dot\n");
    
    // Cleanup
    free_cfg(cfg);
    free_node(ast);
    parser_destroy(parser);
    for (int i = 0; i < token_count; i++) {
        free(tokens[i].value);
    }
}

void test_arrays_and_booleans() {
    const char* code =
        "int test_arrays() {\n"
        "    int arr[5];\n"
        "    bool flags[3];\n"
        "    \n"
        "    arr[0] = 10;\n"
        "    arr[1] = 20;\n"
        "    int sum = arr[0] + arr[1];\n"
        "    \n"
        "    flags[0] = true;\n"
        "    flags[1] = false;\n"
        "    \n"
        "    if (flags[0]) {\n"
        "        sum = sum * 2;\n"
        "    }\n"
        "    \n"
        "    return sum;\n"
        "}\n";
    
    printf("\n=== Testing Arrays and Booleans ===\n");
    printf("Source code:\n%s\n", code);
    
    // Tokenize
    Lexer* lexer = lexer_create(code);
    Token tokens[1024];
    int token_count = 0;
    Token token;
    do {
        token = lexer_next_token(lexer);
        tokens[token_count++] = token;
    } while (token.type != TOKEN_EOF && token_count < 1024);
    lexer_destroy(lexer);
    
    // Parse the code
    Parser* parser = parser_create(tokens, token_count);
    Node* ast = parse(parser);
    
    if (!ast) {
        printf("Failed to parse program\n");
        parser_destroy(parser);
        for (int i = 0; i < token_count; i++) {
            free(tokens[i].value);
        }
        return;
    }
    
    // Build CFG
    CFG* cfg = build_cfg_from_ast(ast);
    if (!cfg) {
        printf("Failed to build CFG\n");
        free_node(ast);
        parser_destroy(parser);
        for (int i = 0; i < token_count; i++) {
            free(tokens[i].value);
        }
        return;
    }
    
    // Print CFG
    print_cfg(cfg);
    
    // Generate DOT file
    cfg_to_dot(cfg, "arrays_booleans.dot");
    printf("\nGenerated DOT file: arrays_booleans.dot\n");
    
    // Cleanup
    free_cfg(cfg);
    free_node(ast);
    parser_destroy(parser);
    for (int i = 0; i < token_count; i++) {
        free(tokens[i].value);
    }
}

int main() {
    printf("=== C Parser CFG Builder Test ===\n\n");
    
    test_simple_function();
    test_if_statement();
    test_while_loop();
    test_for_loop();
    test_arrays_and_booleans();
    
    printf("\n=== All tests completed ===\n");
    return 0;
}
#define _GNU_SOURCE  // For strdup
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "ast.h"

// A simple recursive AST printer for visualization
void print_ast(Node* node, int indent);

// Function to read entire file into a string
char* read_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        return NULL;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Allocate buffer
    char* buffer = malloc(file_size + 1);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(file);
        return NULL;
    }
    
    // Read file
    size_t bytes_read = fread(buffer, 1, file_size, file);
    buffer[bytes_read] = '\0';
    
    fclose(file);
    return buffer;
}

int main(int argc, char* argv[]) {
    char* source_code = NULL;
    
    if (argc > 1) {
        // Read from file
        source_code = read_file(argv[1]);
        if (!source_code) {
            return 1;
        }
        printf("Parsing file: %s\n", argv[1]);
    } else {
        // Use default test code
        printf("No file specified. Using default test code.\n");
        printf("Usage: %s <filename.c>\n\n", argv[0]);
        
        const char* default_code =
        "int main() {\n"
        "  int x = 1;\n"
        "  int y = 0;\n"
        "  switch (x) {\n"
        "    case 1:\n"
        "      y = 2;\n"
        "      break;\n"
        "    case 2:\n"
        "      y = 3;\n"
        "    default:\n"
        "      y = 0;\n"
        "  }\n"
        "}\n";
        
        source_code = strdup(default_code);
    }

    // 1. Lexing
    Lexer* lexer = lexer_create(source_code);
    Token tokens[1024]; // Simple fixed-size buffer
    int token_count = 0;
    Token token;
    do {
        token = lexer_next_token(lexer);
        tokens[token_count++] = token;
        // Optional: print tokens to debug
        // printf("Token: %s ('%s')\n", token_type_to_string(token.type), token.value);
    } while (token.type != TOKEN_EOF && token_count < 1024);
    lexer_destroy(lexer);

    // 2. Parsing
    Parser* parser = parser_create(tokens, token_count);
    Node* ast_root = parse(parser);
    parser_destroy(parser);

    // 3. Print AST
    if (ast_root) {
        printf("\n--- Abstract Syntax Tree ---\n");
        print_ast(ast_root, 0);
    } else {
        printf("Parsing failed to produce an AST.\n");
    }

    // 4. Cleanup
    free_node(ast_root);
    for (int i = 0; i < token_count; i++) {
        free(tokens[i].value);
    }
    free(source_code);

    return 0;
}

// Simple AST printer implementation
void print_ast(Node* node, int indent) {
    if (!node) return;
    for (int i = 0; i < indent; ++i) printf("  ");
    printf("Node Type: %d\n", node->type);

    // Example for specific node types
    if (node->type == NODE_PROGRAM) {
        ProgramNode* prog = (ProgramNode*)node;
        for (int i = 0; i < prog->functions->count; i++) {
            print_ast(prog->functions->items[i], indent + 1);
        }
    }
    // Additional node types would be handled here...
}

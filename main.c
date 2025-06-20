#define _GNU_SOURCE  // For strdup
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "cfg.h"
#include "cfg_builder.h"
#include "cfg_utils.h"

// Global debug flag
int debug_mode = 0;

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

// Generate dot filename from source filename
char* generate_dot_filename(const char* source_filename) {
    const char* dot_pos = strrchr(source_filename, '.');
    size_t base_len = dot_pos ? (size_t)(dot_pos - source_filename) : strlen(source_filename);
    
    char* dot_filename = malloc(base_len + 5); // +4 for ".dot" +1 for null terminator
    if (!dot_filename) {
        return NULL;
    }
    
    strncpy(dot_filename, source_filename, base_len);
    dot_filename[base_len] = '\0';
    strcat(dot_filename, ".dot");
    
    return dot_filename;
}

int main(int argc, char* argv[]) {
    char* source_code = NULL;
    char* input_filename = NULL;
    bool generate_dot = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dot") == 0) {
            generate_dot = true;
        } else if (strcmp(argv[i], "--debug") == 0) {
            debug_mode = 1;
        } else if (argv[i][0] != '-') {
            // This is the input filename
            input_filename = argv[i];
        } else {
            printf("Unknown option: %s\n", argv[i]);
            printf("Usage: %s [--dot] [--debug] <filename.c>\n", argv[0]);
            printf("  --dot    Generate a DOT file for CFG visualization\n");
            printf("  --debug  Enable debug output messages\n");
            return 1;
        }
    }
    
    if (input_filename) {
        // Read from file
        source_code = read_file(input_filename);
        if (!source_code) {
            return 1;
        }
        printf("Parsing file: %s\n", input_filename);
    } else {
        // Use default test code
        printf("No file specified. Using default test code.\n");
        printf("Usage: %s [--dot] [--debug] <filename.c>\n", argv[0]);
        printf("  --dot    Generate a DOT file for CFG visualization\n");
        printf("  --debug  Enable debug output messages\n\n");
        
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
        
        // 4. Generate CFG and DOT file if requested
        if (generate_dot) {
            printf("\n--- Generating Control Flow Graph ---\n");
            CFG* cfg = build_cfg_from_ast(ast_root);
            if (cfg) {
                char* dot_filename;
                if (input_filename) {
                    dot_filename = generate_dot_filename(input_filename);
                } else {
                    dot_filename = strdup("default.dot");
                }
                
                if (dot_filename) {
                    cfg_to_dot(cfg, dot_filename);
                    printf("Generated DOT file: %s\n", dot_filename);
                    printf("To visualize: dot -Tpng %s -o %s.png\n", dot_filename,
                           dot_filename); // This will create filename.dot.png
                    
                    // Also print CFG to console
                    printf("\n--- Control Flow Graph ---\n");
                    print_cfg(cfg);
                    
                    free(dot_filename);
                } else {
                    printf("Error: Failed to generate dot filename\n");
                }
                
                free_cfg(cfg);
            } else {
                printf("Error: Failed to build CFG from AST\n");
            }
        }
    } else {
        printf("Parsing failed to produce an AST.\n");
    }

    // 5. Cleanup
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

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
#include "hw_analyzer.h"
#include "cfg_to_microcode.h"
#include "ast_to_microcode.h"
#include "ssa_optimizer.h"
#include "verilog_generator.h"
#include "preprocessor.h"

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
    bool analyze_hardware = false;
    bool generate_microcode = false;
    bool generate_verilog = false;
    bool generate_testbench = false;
    bool generate_all_hdl = false;
    bool optimize_ssa = false;
    
    // Microcode generation modes
    typedef enum {
        MICROCODE_NONE,        // No microcode generation
        MICROCODE_SSA,         // SSA-based (current default)
        MICROCODE_COMPACT      // Statement-based (hotstate compatible)
    } MicrocodeMode;
    
    MicrocodeMode microcode_mode = MICROCODE_NONE;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dot") == 0) {
            generate_dot = true;
        } else if (strcmp(argv[i], "--debug") == 0) {
            debug_mode = 1;
            printf("DEBUG: main.c: debug_mode set to %d\n", debug_mode); // Direct print for debugging
        } else if (strcmp(argv[i], "--hardware") == 0) {
            analyze_hardware = true;
        } else if (strcmp(argv[i], "--microcode") == 0) {
            generate_microcode = true;
            microcode_mode = MICROCODE_SSA; // Default to SSA mode for backward compatibility
        } else if (strcmp(argv[i], "--microcode-ssa") == 0) {
            generate_microcode = true;
            microcode_mode = MICROCODE_SSA;
        } else if (strcmp(argv[i], "--opt") == 0) {
            optimize_ssa = true;
        } else if (strcmp(argv[i], "--microcode-hs") == 0) {
            generate_microcode = true;
            microcode_mode = MICROCODE_COMPACT;
        } else if (strcmp(argv[i], "--verilog") == 0) {
            generate_verilog = true;
        } else if (strcmp(argv[i], "--testbench") == 0) {
            generate_testbench = true;
        } else if (strcmp(argv[i], "--all-hdl") == 0) {
            generate_all_hdl = true;
        } else if (argv[i][0] != '-') {
            // This is the input filename
            input_filename = argv[i];
        } else {
            printf("Unknown option: %s\n", argv[i]);
            printf("Usage: %s [options] <filename.c>\n", argv[0]);
            printf("Options:\n");
            printf("  --dot                Generate a DOT file for CFG visualization\n");
            printf("  --debug              Enable debug output messages\n");
            printf("  --hardware           Analyze hardware constructs (state/input variables)\n");
            printf("  --microcode          Generate SSA-based microcode (default mode)\n");
            printf("  --microcode-ssa      Generate SSA-based microcode (verbose, for analysis)\n");
            printf("  --microcode-hs       Generate hotstate-compatible microcode\n");
            printf("  --opt                Apply SSA optimizations (constant/copy propagation)\n");
            printf("  --verilog            Generate Verilog HDL module\n");
            printf("  --testbench          Generate Verilog testbench\n");
            printf("  --all-hdl            Generate all HDL files (module, testbench, stimulus, makefile)\n");
            return 1;
        }
    }
    
    if (input_filename) {
        // Preprocess includes and read from file
        source_code = preprocess_includes(input_filename);
        if (!source_code) {
            printf("Error: Failed to preprocess file '%s'\n", input_filename);
            return 1;
        }
        printf("Parsing file: %s\n", input_filename);
    } else {
        // Use default test code
        printf("No file specified. Using default test code.\n");
        printf("Usage: %s [options] <filename.c>\n", argv[0]);
        printf("Options:\n");
        printf("  --dot                Generate a DOT file for CFG visualization\n");
        printf("  --debug              Enable debug output messages\n");
        printf("  --hardware           Analyze hardware constructs (state/input variables)\n");
        printf("  --microcode          Generate SSA-based microcode (default mode)\n");
        printf("  --microcode-ssa      Generate SSA-based microcode (verbose, for analysis)\n");
        printf("  --microcode-hs       Generate hotstate-compatible microcode\n");
        printf("  --opt                Apply SSA optimizations (constant/copy propagation)\n");
        printf("  --verilog            Generate Verilog HDL module\n");
        printf("  --testbench          Generate Verilog testbench\n");
        printf("  --all-hdl            Generate all HDL files (module, testbench, stimulus, makefile)\n\n");
        
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
        
        // 5. Hardware Analysis if requested
        if (analyze_hardware) {
            printf("\n--- Hardware Analysis ---\n");
            HardwareContext* hw_ctx = analyze_hardware_constructs(ast_root);
            if (hw_ctx) {
                print_hardware_context(hw_ctx, stdout);
                free_hardware_context(hw_ctx);
            } else {
                printf("Error: Failed to analyze hardware constructs\n");
            }
        }
        
        // 6. Microcode Generation if requested
        if (generate_microcode) {
            // First analyze hardware constructs
            HardwareContext* hw_ctx = analyze_hardware_constructs(ast_root);
            if (!hw_ctx) {
                printf("Error: Failed to analyze hardware constructs\n");
            } else {
                switch (microcode_mode) {
                    case MICROCODE_SSA:
                        if (optimize_ssa) {
                            printf("\n--- Generating Optimized SSA-Based Microcode ---\n");
                        } else {
                            printf("\n--- Generating SSA-Based Microcode ---\n");
                        }
                        {
                            // Build CFG for SSA-based generation
                            CFG* cfg = build_cfg_from_ast(ast_root);
                            if (!cfg) {
                                printf("Error: Failed to build CFG from AST\n");
                            } else {
                                // Apply SSA optimizations if requested
                                if (optimize_ssa) {
                                    cfg = optimize_ssa_cfg(cfg, hw_ctx);
                                }
                                
                                // Generate SSA-based microcode
                                HotstateMicrocode* microcode = cfg_to_hotstate_microcode(cfg, hw_ctx);
                                if (microcode) {
                                    // Print microcode table
                                    print_compact_microcode_table(microcode, stdout);
                                    
                                    // Print analysis
                                    print_microcode_analysis(microcode, stdout);
                                    
                                    // Generate memory files if input filename provided
                                    if (input_filename) {
                                        generate_all_output_files(microcode, input_filename);
                                    }
                                    
                                    free_hotstate_microcode(microcode);
                                } else {
                                    if (optimize_ssa) {
                                        printf("Error: Failed to generate optimized SSA-based microcode\n");
                                    } else {
                                        printf("Error: Failed to generate SSA-based microcode\n");
                                    }
                                }
                                
                                free_cfg(cfg);
                            }
                        }
                        break;
                        
                    case MICROCODE_COMPACT:
                        printf("\n--- Generating Hotstate-Compatible Microcode ---\n");
                        {
                            CompactMicrocode* compact_mc = ast_to_compact_microcode(ast_root, hw_ctx);
                            if (compact_mc) {
                                // Print compact microcode table
                                print_compact_microcode_table(compact_mc, stdout);
                                
                                // Print analysis
                                print_compact_microcode_analysis(compact_mc, stdout);
                                
                                free_compact_microcode(compact_mc);
                            } else {
                                printf("Error: Failed to generate compact microcode\n");
                            }
                        }
                        break;
                        
                    default:
                        printf("Error: Invalid microcode generation mode\n");
                        break;
                }
                
                free_hardware_context(hw_ctx);
            }
        }
        
        // 7. Verilog HDL Generation if requested
        if (generate_verilog || generate_testbench || generate_all_hdl) {
            printf("\n--- Generating Verilog HDL ---\n");
            
            // First analyze hardware constructs
            HardwareContext* hw_ctx = analyze_hardware_constructs(ast_root);
            if (!hw_ctx) {
                printf("Error: Failed to analyze hardware constructs\n");
            } else {
                // Build CFG if not already done
                CFG* cfg = build_cfg_from_ast(ast_root);
                if (!cfg) {
                    printf("Error: Failed to build CFG from AST\n");
                } else {
                    // Generate microcode (needed for HDL generation)
                    HotstateMicrocode* microcode = cfg_to_hotstate_microcode(cfg, hw_ctx);
                    if (microcode) {
                        // Set up Verilog generation options
                        VerilogGenOptions options = {
                            .generate_module = generate_verilog || generate_all_hdl,
                            .generate_testbench = generate_testbench || generate_all_hdl,
                            .generate_user_stim = generate_all_hdl,
                            .generate_makefile = generate_all_hdl,
                            .generate_all = generate_all_hdl
                        };
                        
                        // Generate Verilog HDL
                        generate_verilog_hdl(microcode, input_filename ? input_filename : "output", &options);
                        
                        free_hotstate_microcode(microcode);
                    } else {
                        printf("Error: Failed to generate microcode for HDL generation\n");
                    }
                    
                    free_cfg(cfg);
                }
                
                free_hardware_context(hw_ctx);
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

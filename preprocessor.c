#define _GNU_SOURCE  // For strdup
#include "preprocessor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h>

// Helper function to read entire file into a string
static char* read_file_content(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        return NULL;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Allocate buffer
    char* buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    
    // Read file
    size_t bytes_read = fread(buffer, 1, file_size, file);
    buffer[bytes_read] = '\0';
    
    fclose(file);
    return buffer;
}

// Helper function to check if file exists
static int file_exists(const char* filename) {
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

// Helper function to resolve include path
static char* resolve_include_path(const char* include_filename, const char* current_file_dir) {
    char* resolved_path = NULL;
    
    // Try relative to current file directory first
    if (current_file_dir) {
        size_t dir_len = strlen(current_file_dir);
        size_t file_len = strlen(include_filename);
        resolved_path = malloc(dir_len + file_len + 2); // +2 for '/' and '\0'
        sprintf(resolved_path, "%s/%s", current_file_dir, include_filename);
        
        if (file_exists(resolved_path)) {
            return resolved_path;
        }
        free(resolved_path);
    }
    
    // Try current working directory
    if (file_exists(include_filename)) {
        return strdup(include_filename);
    }
    
    return NULL;
}

// Simple text-based include processing
static char* process_includes_simple(const char* filename, char** included_files, int* included_count, int max_includes) {
    // Check for include cycles
    for (int i = 0; i < *included_count; i++) {
        if (strcmp(included_files[i], filename) == 0) {
            fprintf(stderr, "Warning: Circular include detected for '%s'\n", filename);
            return strdup(""); // Return empty string to avoid infinite recursion
        }
    }
    
    // Check maximum include depth
    if (*included_count >= max_includes) {
        fprintf(stderr, "Error: Maximum include depth exceeded\n");
        return NULL;
    }
    
    // Add current file to included list
    included_files[*included_count] = strdup(filename);
    (*included_count)++;
    
    // Read the file content
    char* content = read_file_content(filename);
    if (!content) {
        fprintf(stderr, "Error: Cannot read file '%s'\n", filename);
        return NULL;
    }
    
    // Get directory of current file for relative includes
    char* filename_copy = strdup(filename);
    char* current_dir = dirname(filename_copy);
    
    // Process line by line without modifying original content
    char* content_copy = strdup(content);
    char* result = malloc(strlen(content) * 2 + 1000); // Extra space for includes
    result[0] = '\0';
    
    char* line_start = content_copy;
    char* line_end;
    
    while ((line_end = strchr(line_start, '\n')) != NULL || *line_start != '\0') {
        // Extract current line
        char* line;
        if (line_end) {
            *line_end = '\0';
            line = line_start;
            line_start = line_end + 1;
        } else {
            // Last line without newline
            line = line_start;
            line_start += strlen(line_start);
        }
        
        // Check if line starts with #include
        char* trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++; // Skip whitespace
        
        if (strncmp(trimmed, "#include", 8) == 0) {
            // Extract filename from #include "filename"
            char* quote_start = strchr(trimmed, '"');
            if (quote_start) {
                quote_start++; // Skip opening quote
                char* quote_end = strchr(quote_start, '"');
                if (quote_end) {
                    *quote_end = '\0'; // Null terminate filename
                    
                    // Resolve and process included file
                    char* include_path = resolve_include_path(quote_start, current_dir);
                    if (include_path) {
                        char* included_content = process_includes_simple(include_path, included_files, included_count, max_includes);
                        if (included_content) {
                            strcat(result, included_content);
                            strcat(result, "\n");
                            free(included_content);
                        }
                        free(include_path);
                    } else {
                        fprintf(stderr, "Warning: Cannot find include file '%s'\n", quote_start);
                    }
                }
            }
        } else {
            // Regular line, copy as-is
            strcat(result, line);
            strcat(result, "\n");
        }
    }
    
    free(content_copy);
    
    free(content);
    free(filename_copy);
    
    return result;
}

char* preprocess_includes(const char* filename) {
    const int MAX_INCLUDES = 100;
    char* included_files[MAX_INCLUDES];
    int included_count = 0;
    
    char* result = process_includes_simple(filename, included_files, &included_count, MAX_INCLUDES);
    
    // Clean up included files list
    for (int i = 0; i < included_count; i++) {
        free(included_files[i]);
    }
    
    return result;
}
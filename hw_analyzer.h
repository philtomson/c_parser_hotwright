#ifndef HW_ANALYZER_H
#define HW_ANALYZER_H

#include "ast.h"
#include <stdbool.h>

// Hardware variable types
typedef enum {
    HW_VAR_STATE,    // Output state variable (LED0, LED1, etc.)
    HW_VAR_INPUT,    // Input variable (a0, a1, etc.)
    HW_VAR_UNKNOWN   // Not a hardware variable
} HardwareVarType;

// State variable information
typedef struct {
    char* name;              // Variable name (e.g., "LED0")
    int state_number;        // State number from comment (e.g., 0 from /* state0 */)
    bool initial_value;      // Initial value (0 or 1)
    VarDeclNode* ast_node;   // Reference to AST node
} StateVariable;

// Input variable information  
typedef struct {
    char* name;              // Variable name (e.g., "a0")
    int input_number;        // Auto-assigned input number
    VarDeclNode* ast_node;   // Reference to AST node
} InputVariable;

// Complete hardware context
typedef struct {
    // State variables (outputs)
    StateVariable* states;
    int state_count;
    int state_capacity;
    
    // Input variables
    InputVariable* inputs;
    int input_count;
    int input_capacity;
    
    // Quick lookup tables
    char** all_var_names;    // All variable names for quick lookup
    HardwareVarType* var_types; // Corresponding types
    int total_var_count;
    
    // Analysis results
    bool analysis_successful;
    char* error_message;
} HardwareContext;

// --- Core API Functions ---

// Main analysis function
HardwareContext* analyze_hardware_constructs(Node* ast);

// Variable classification
HardwareVarType classify_variable(VarDeclNode* var_decl);
bool is_state_variable(VarDeclNode* var_decl);
bool is_input_variable(VarDeclNode* var_decl);

// State number extraction (for future comment support)
bool is_common_input_name(const char* var_name);

// Lookup functions
int get_state_number_by_name(HardwareContext* ctx, const char* var_name);
int get_input_number_by_name(HardwareContext* ctx, const char* var_name);
HardwareVarType get_variable_type(HardwareContext* ctx, const char* var_name);

// Validation functions
bool validate_hardware_context(HardwareContext* ctx);
bool check_state_number_conflicts(HardwareContext* ctx);
bool check_variable_name_conflicts(HardwareContext* ctx);

// Memory management
void free_hardware_context(HardwareContext* ctx);

// Debug and output
void print_hardware_context(HardwareContext* ctx, FILE* output);
void print_state_variables(HardwareContext* ctx, FILE* output);
void print_input_variables(HardwareContext* ctx, FILE* output);

#endif // HW_ANALYZER_H
#define _GNU_SOURCE  // For strdup
#include "microcode_defs.h" // Explicitly include for Code struct definition
#include "ast_to_microcode.h"
#include "hw_analyzer.h"       // For HardwareContext and HardwareStateVar
#include "lexer.h"             // For TOKEN_
#include "cfg_to_microcode.h"  // For HotstateMicrocode and HOTSTATE_ macros
#include "expression_evaluator.h" // New: For SimulatedExpression and evaluator functions
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h> // For fprintf


typedef struct {
    const char* header;
    int width;
    int active;
} ColumnFormat;

// Global variables for hotstate compatibility
static int gSwitches = 0;

// Forward declarations
static void process_function(CompactMicrocode* mc, FunctionDefNode* func);
static void process_statement(CompactMicrocode* mc, Node* stmt, int* addr);
static void add_compact_instruction(CompactMicrocode* mc, MCode* mcode, const char* label, JumpType jump_type, int jump_target_param);
static void resolve_compact_microcode_jumps(CompactMicrocode* mc);
static void resolve_switch_break_addresses(CompactMicrocode* mc);
// static uint32_t encode_compact_instruction(int state, int var, int timer, int jump,
//                                           int switch_val, int timer_val, int cap,
//                                           int var_val, int branch, int force, int ret);
//static char* create_statement_label(Node* stmt);
static char* create_condition_label(Node* condition);

// New function to populate MCode struct
static void populate_mcode_instruction(
    CompactMicrocode* mc,
    MCode* mcode_ptr,
    uint32_t state,
    uint32_t mask,
    uint32_t jadr,
    uint32_t varSel,
    uint32_t timerSel,
    uint32_t timerLd,
    uint32_t switch_sel,
    uint32_t switch_adr,
    uint32_t state_capture,
    uint32_t var_or_timer,
    uint32_t branch,
    uint32_t forced_jmp,
    uint32_t sub,
    uint32_t rtn
);
static int calculate_jump_address(CompactMicrocode* mc, IfNode* if_node, int current_addr);
static int calculate_else_jump_address(CompactMicrocode* mc, IfNode* if_node, int current_addr);
static int estimate_statement_size(Node* stmt);
static void process_assignment(CompactMicrocode* mc, AssignmentNode* assign, int* addr);
static void process_expression_statement(CompactMicrocode* mc, ExpressionStatementNode* expr_stmt, int* addr);
static void process_switch_statement(CompactMicrocode* mc, SwitchNode* switch_node, int* addr);
static void process_expression(CompactMicrocode* mc, Node* expr, int* addr);
static void generate_expected_sequence(CompactMicrocode* mc, int* addr);
static void process_for_loop(CompactMicrocode* mc, ForNode* for_node, int* addr);
// Hotstate compatibility functions
static int get_state_number_for_variable(const char* var_name, HardwareContext* hw_ctx);
static void calculate_hotstate_fields(AssignmentNode* assign, HardwareContext* hw_ctx, int* state_out, int* mask_out);
static void calculate_comma_expression_fields(Node* comma_expr, HardwareContext* hw_ctx, int* state_out, int* mask_out);

// Switch management functions
static void populate_switch_memory(CompactMicrocode* mc, int switch_id, SwitchNode* switch_node, int* addr);
static void add_switch_instruction(CompactMicrocode* mc, MCode* mcode, const char* label, int switch_id);
static void add_case_instruction(CompactMicrocode* mc, MCode* mcode, const char* label, int switch_id);
// static uint32_t encode_switch_instruction(int switch_id, int is_switch_instr);
static void push_context(CompactMicrocode* mc, LoopSwitchContext* context);
static void pop_context(CompactMicrocode* mc);
static void add_pending_jump(CompactMicrocode* mc, int instruction_index, int target_instruction_address, bool is_exit_jump);
static void resize_pending_jumps(CompactMicrocode* mc);
static void add_conditional_expression(CompactMicrocode* mc, Node* expression_node, int varsel_id);
static void resize_conditional_expressions(CompactMicrocode* mc);
static bool is_simple_variable_reference(Node* expr);
static bool is_complex_boolean_expression(Node* expr);

static void push_context(CompactMicrocode* mc, LoopSwitchContext* context) {
    if(mc->stack_ptr >= mc->stack_capacity) {
        mc->stack_capacity *= 2;
        mc->loop_switch_stack = realloc(mc->loop_switch_stack, sizeof(LoopSwitchContext) * mc->stack_capacity);
        // Error handling for realloc is important in production code, but for this context,
        // we'll assume it succeeds for simplicity or let it crash if it's truly out of memory.
    }
    mc->loop_switch_stack[mc->stack_ptr++] = *context;
}

static void add_pending_jump(CompactMicrocode* mc, int instruction_index, int target_instruction_address, bool is_exit_jump) {
    if (mc->pending_jump_count >= mc->pending_jump_capacity) {
        resize_pending_jumps(mc);
    }
    mc->pending_jumps[mc->pending_jump_count].instruction_index = instruction_index;
    mc->pending_jumps[mc->pending_jump_count].target_instruction_address = target_instruction_address;
    mc->pending_jumps[mc->pending_jump_count].is_exit_jump = is_exit_jump;
    mc->pending_jump_count++;
}
 
 static void resize_pending_jumps(CompactMicrocode* mc) {
     mc->pending_jump_capacity *= 2;
     mc->pending_jumps = realloc(mc->pending_jumps, sizeof(PendingJump) * mc->pending_jump_capacity);
     if (mc->pending_jumps == NULL) {
         fprintf(stderr, "Error: Failed to reallocate pending_jumps array.\n");
         exit(EXIT_FAILURE); // Or handle error appropriately
     }
 }

 static void resize_conditional_expressions(CompactMicrocode* mc) {
     mc->conditional_expression_capacity *= 2;
     mc->conditional_expressions = realloc(mc->conditional_expressions, sizeof(ConditionalExpressionInfo) * mc->conditional_expression_capacity);
     if (mc->conditional_expressions == NULL) {
         fprintf(stderr, "Error: Failed to reallocate conditional_expressions array.\n");
         exit(EXIT_FAILURE);
     }
 }

 static void add_conditional_expression(CompactMicrocode* mc, Node* expression_node, int varsel_id) {
     if (mc->conditional_expression_count >= mc->conditional_expression_capacity) {
         resize_conditional_expressions(mc);
     }
     mc->conditional_expressions[mc->conditional_expression_count].expression_node = expression_node;
     mc->conditional_expressions[mc->conditional_expression_count].varsel_id = varsel_id;
     mc->conditional_expressions[mc->conditional_expression_count].sim_expr = NULL; // Will be populated later
     mc->conditional_expression_count++;
 }

// Check if expression is a simple variable reference (no operators)
static bool is_simple_variable_reference(Node* expr) {
    if (!expr) return false;
    
    // ONLY simple identifier reference like "a0" or "input1" with NO operators
    if (expr->type == NODE_IDENTIFIER) {
        return true;
    }
    
    return false;
}

// Check if expression is complex and needs a LUT entry
// Based on hotstate behavior: anything other than a bare variable reference needs a varSel
static bool is_complex_boolean_expression(Node* expr) {
    if (!expr) return false;

    // Any binary operation needs a LUT (AND, OR, ==, !=, etc.)
    if (expr->type == NODE_BINARY_OP) {
        return true;
    }

    // Unary NOT also needs a LUT entry (even !(a0) needs evaluation)
    if (expr->type == NODE_UNARY_OP) {
        return true;
    }

    // Number literals in complex contexts need LUT entries for constant evaluation
    // But simple constants like while(1) don't need varSel at all
    if (expr->type == NODE_NUMBER_LITERAL) {
        return true; // Still mark as complex for LUT purposes
    }

    return false;
}

// Check if expression is a constant that doesn't need varSel (like while(1))
static bool is_constant_condition(Node* expr) {
    if (!expr) return true; // No condition is a constant

    if (expr->type == NODE_NUMBER_LITERAL) {
        // Check if it's a simple constant (0 or 1)
        NumberLiteralNode* num_node = (NumberLiteralNode*)expr;
        return (strcmp(num_node->value, "0") == 0 || strcmp(num_node->value, "1") == 0);
    }

    return false;
}

// Hybrid varSel assignment: simple variables = 0, complex expressions = incremental
static int get_hybrid_varsel(Node* condition, CompactMicrocode* mc) {
    if (is_constant_condition(condition)) {
        return 0; // Constant conditions (like while(1)) don't need varSel
    } else if (is_simple_variable_reference(condition)) {
        return 0; // Direct hardware input (c-parser efficiency)
    } else if (is_complex_boolean_expression(condition)) {
        mc->has_complex_conditionals = 1; // Mark that we need LUT
        return mc->var_sel_counter++; // Use incremental counter like hotstate
    } else {
        return 0; // Fallback for edge cases
    }
}

static void pop_context(CompactMicrocode* mc){
    if (mc->stack_ptr > 0) {
        mc->stack_ptr--;
    } else {
        fprintf(stderr, "Warning: Attempted to pop from empty loop/switch stack.\n");
    }
}
    

static LoopSwitchContext peek_context(CompactMicrocode* mc, ContextSearchType search_type) {
    for (int i = mc->stack_ptr - 1; i >= 0; i--) {
        LoopSwitchContext current_context = mc->loop_switch_stack[i];
        if (search_type == CONTEXT_TYPE_LOOP_OR_SWITCH &&
            (current_context.loop_type == NODE_WHILE ||
             current_context.loop_type == NODE_FOR ||
             current_context.loop_type == NODE_SWITCH)) {
            return current_context;
        } else if (search_type == CONTEXT_TYPE_LOOP &&
                   (current_context.loop_type == NODE_WHILE ||
                    current_context.loop_type == NODE_FOR)) {
            return current_context;
        }
    }
    fprintf(stderr, "Error: 'break' or 'continue' statement outside of a valid context.\n");
    LoopSwitchContext error_context = {-1, -1}; // Return invalid targets
    return error_context;
}

// Implement populate_mcode_instruction function
static void populate_mcode_instruction(
    CompactMicrocode* mc,
    MCode* mcode_ptr,
    uint32_t state,
    uint32_t mask,
    uint32_t jadr_placeholder, // Renamed to clarify it's a placeholder
    uint32_t varSel,
    uint32_t timerSel,
    uint32_t timerLd,
    uint32_t switch_sel,
    uint32_t switch_adr,
    uint32_t state_capture,
    uint32_t var_or_timer,
    uint32_t branch,
    uint32_t forced_jmp,
    uint32_t sub,
    uint32_t rtn
) {
    // Populate the MCode struct fields directly
    mcode_ptr->state = state;
    mcode_ptr->mask = mask;
    mcode_ptr->jadr = jadr_placeholder; // Set to placeholder, will be resolved in Pass 2
    mcode_ptr->varSel = varSel;
    mcode_ptr->timerSel = timerSel;
    mcode_ptr->timerLd = timerLd;
    mcode_ptr->switch_sel = switch_sel;
    mcode_ptr->switch_adr = switch_adr;
    mcode_ptr->state_capture = state_capture;
    mcode_ptr->var_or_timer = var_or_timer;
    mcode_ptr->branch = branch;
    mcode_ptr->forced_jmp = forced_jmp;
    mcode_ptr->sub = sub;
    mcode_ptr->rtn = rtn;

    // Update max_val fields in CompactMicrocode for dynamic bit-width calculation
    // Update max_jadr_val based on the potential maximum address, not placeholder
    // This assumes actual addresses are within the range of potential jadr values
    // A more robust solution might track max_jadr in the resolve pass.
    // For now, we'll keep it as is, but it's important to note this might not be accurate
    // until after resolution.
    // if (jadr_placeholder > mc->max_jadr_val) {
    //     mc->max_jadr_val = jadr_placeholder;
    // }
    if (varSel > mc->max_varsel_val) {
        mc->max_varsel_val = varSel;
    }
    if (state > mc->max_state_val) {
        mc->max_state_val = state;
    }
    // Add similar updates for other fields that need dynamic sizing as they become relevant
    if (mask > mc->max_mask_val) {
        mc->max_mask_val = mask;
    }
    if (timerSel > mc->max_timersel_val) {
        mc->max_timersel_val = timerSel;
    }
    if (timerLd > mc->max_timerld_val) {
        mc->max_timerld_val = timerLd;
    }
    if (switch_sel > mc->max_switch_sel_val) {
        mc->max_switch_sel_val = switch_sel;
    }
    if (switch_adr > mc->max_switch_adr_val) {
        mc->max_switch_adr_val = switch_adr;
    }
    if (state_capture > mc->max_state_capture_val) {
        mc->max_state_capture_val = state_capture;
    }
    if (var_or_timer > mc->max_var_or_timer_val) {
        mc->max_var_or_timer_val = var_or_timer;
    }
    if (branch > mc->max_branch_val) {
        mc->max_branch_val = branch;
    }
    if (forced_jmp > mc->max_forced_jmp_val) {
        mc->max_forced_jmp_val = forced_jmp;
    }
    if (sub > mc->max_sub_val) {
        mc->max_sub_val = sub;
    }
    if (rtn > mc->max_rtn_val) {
        mc->max_rtn_val = rtn;
    }
}

CompactMicrocode* ast_to_compact_microcode(Node* ast_root, HardwareContext* hw_ctx) {
    if (!ast_root || ast_root->type != NODE_PROGRAM) {
        return NULL;
    }
    
    // Initialize global counters like hotstate
    // TODO: eliminate globals by moving to CompactMicrocode!
    gSwitches = 0; //TODO: mc->switch_count
    
    CompactMicrocode* mc = malloc(sizeof(CompactMicrocode));
    mc->instructions = (Code*)malloc(sizeof(mc->instructions[0]) * 32);
    mc->instruction_count = 0;
    mc->timer_count = 0;
    mc->instruction_capacity = 32;
    mc->function_name = strdup("main"); //TODO: this needs to not be hardcoded
    mc->hw_ctx = hw_ctx;
    
    // Initialize switch memory management
    mc->switchmem = calloc(MAX_SWITCH_ENTRIES * MAX_SWITCHES, sizeof(uint32_t));
    mc->switch_count = 0;
    mc->switch_offset_bits = SWITCH_OFFSET_BITS;
    
    mc->state_assignments = 0;
    mc->branch_instructions = 0;
    mc->jump_instructions = 0;
    mc->state_assignments = 0;
    mc->branch_instructions = 0;
    mc->jump_instructions = 0;
    mc->max_jadr_val = 0;
    mc->max_varsel_val = 0;
    mc->var_sel_counter = 1; // Initialize to 1 like hotstate (hybrid approach)
    mc->has_complex_conditionals = 0; // Track if any complex expressions need LUT
    mc->max_state_val = 0;
    mc->max_mask_val = 0; // Initialize new fields
    mc->max_timersel_val = 0;
    mc->max_timerld_val = 0;
    mc->max_switch_sel_val = 0;
    mc->max_switch_adr_val = 0;
    mc->max_state_capture_val = 0;
    mc->max_var_or_timer_val = 0;
    mc->max_branch_val = 0;
    mc->max_forced_jmp_val = 0;
    mc->max_sub_val = 0;
    mc->max_rtn_val = 0;

    // Initialize Uber LUT (vardata) management
    mc->conditional_expressions = (ConditionalExpressionInfo*)malloc(sizeof(ConditionalExpressionInfo) * 16); // Initial capacity
    if (mc->conditional_expressions == NULL) {
        fprintf(stderr, "Error: Failed to allocate conditional_expressions.\n");
        free(mc->instructions);
        free(mc->switchmem);
        free(mc);
        return NULL;
    }
    mc->conditional_expression_count = 0;
    mc->conditional_expression_capacity = 16;
    mc->vardata_lut = NULL; // Will be allocated later
    mc->vardata_lut_size = 0;

    // Initialize pending jump resolution
    mc->pending_jumps = (PendingJump*)malloc(sizeof(PendingJump) * 16); // Initial capacity
    if (mc->pending_jumps == NULL) {
        fprintf(stderr, "Error: Failed to allocate pending_jumps.\n");
        free(mc->instructions);
        free(mc->switchmem);
        free(mc->conditional_expressions); // Free conditional_expressions as well
        free(mc);
        return NULL;
    }
    mc->pending_jump_count = 0;
    mc->pending_jump_capacity = 16;


    // Initialize pending switch break resolution
    mc->pending_switch_breaks = (PendingSwitchBreak*)malloc(sizeof(PendingSwitchBreak) * MAX_PENDING_SWITCH_BREAKS);
    if (mc->pending_switch_breaks == NULL) {
        fprintf(stderr, "Error: Failed to allocate pending_switch_breaks.\n");
        free(mc->pending_jumps);
        free(mc->instructions);
        free(mc->switchmem);
        free(mc);
        return NULL;
    }
    mc->pending_switch_break_count = 0;

    // Initialize switch info tracking
    mc->switch_infos = (SwitchInfo*)malloc(sizeof(SwitchInfo) * MAX_SWITCHES);
    if (mc->switch_infos == NULL) {
        fprintf(stderr, "Error: Failed to allocate switch_infos.\n");
        free(mc->pending_switch_breaks);
        free(mc->pending_jumps);
        free(mc->instructions);
        free(mc->switchmem);
        free(mc);
        return NULL;
    }
    mc->switch_info_count = 0;
    mc->switch_info_capacity = MAX_SWITCHES;

    // Initialize loop/switch stack
    mc->stack_ptr = 0;
    mc->stack_capacity = 16; // Initial capacity
    mc->loop_switch_stack = malloc(sizeof(LoopSwitchContext) * mc->stack_capacity);
    if (mc->loop_switch_stack == NULL) {
        fprintf(stderr, "Error: Failed to allocate loop_switch_stack.\n");
        free(mc->instructions);
        free(mc->switchmem);
        free(mc->pending_jumps); // Free pending_jumps as well
        free(mc);
        return NULL;
    }
    mc->pending_jump_count = 0;
    mc->pending_jump_capacity = 16;
    
    ProgramNode* program = (ProgramNode*)ast_root;
    
    // Find main function
    for (int i = 0; i < program->functions->count; i++) {
        Node* func_node = program->functions->items[i];
        if (func_node->type == NODE_FUNCTION_DEF) {
            FunctionDefNode* func = (FunctionDefNode*)func_node;
            if (strcmp(func->name, "main") == 0) {
                process_function(mc, func);
                break;
            }
        }
    }
    
    // Resolve all pending jumps after all microcode has been generated
    resolve_compact_microcode_jumps(mc);
    
    // Resolve switch break addresses after all microcode has been generated
    resolve_switch_break_addresses(mc);
    
    // Phase 2.3.2: Call create_simulated_expression and eval_simulated_expression for collected conditional expressions
    // This needs to happen after all microcode is generated and addresses are resolved,
    // as evaluation might depend on final instruction counts or addresses for context.
    if (mc->has_complex_conditionals && mc->conditional_expression_count > 0) {
        print_debug("DEBUG: Evaluating %d conditional expressions for Uber LUT.\n", mc->conditional_expression_count);
        // Determine the total number of input variables in the hardware context
        // This is needed to calculate the full LUT size (2^num_total_input_vars)
        int num_total_input_vars = mc->hw_ctx->input_count; // Assuming input_count is the correct measure
        print_debug("DEBUG: num_total_input_vars: %d\n", num_total_input_vars);

        for (int i = 0; i < mc->conditional_expression_count; i++) {
            ConditionalExpressionInfo* info = &mc->conditional_expressions[i];
            print_debug("DEBUG: Creating and evaluating simulated expression for varsel_id %d.\n", info->varsel_id);
            info->sim_expr = create_simulated_expression(info->expression_node, mc->hw_ctx);
            if (info->sim_expr) {
                eval_simulated_expression(info->sim_expr, mc->hw_ctx, num_total_input_vars);
                print_debug("DEBUG: sim_expr->LUT_size for varsel_id %d: %d\n", info->varsel_id, info->sim_expr->LUT_size);
                print_debug("DEBUG: sim_expr->dependent_input_mask for varsel_id %d: 0x%x\n", info->varsel_id, info->sim_expr->dependent_input_mask);
                print_debug("DEBUG: sim_expr->LUT for varsel_id %d: ", info->varsel_id);
                for (int j = 0; j < info->sim_expr->LUT_size; j++) {
                    fprintf(stderr, "%d ", info->sim_expr->LUT[j]);
                }
                fprintf(stderr, "\n");
            } else {
                fprintf(stderr, "Error: Failed to create simulated expression for varsel_id %d.\n", info->varsel_id);
            }
        }

        // Allocate and populate vardata_lut
        // Need to allocate enough space for the highest varsel_id, not just the count
        int max_varsel_id = 0;
        for (int i = 0; i < mc->conditional_expression_count; i++) {
            if (mc->conditional_expressions[i].varsel_id > max_varsel_id) {
                max_varsel_id = mc->conditional_expressions[i].varsel_id;
            }
        }
        mc->vardata_lut_size = (max_varsel_id + 1) * (1 << num_total_input_vars);
        mc->vardata_lut = (uint8_t*)malloc(sizeof(uint8_t) * mc->vardata_lut_size);
        if (!mc->vardata_lut) {
            fprintf(stderr, "Error: Failed to allocate vardata_lut.\n");
            exit(EXIT_FAILURE);
        }
        print_debug("DEBUG: Allocated vardata_lut of size: %d\n", mc->vardata_lut_size);

        for (int i = 0; i < mc->conditional_expression_count; i++) {
            ConditionalExpressionInfo* info = &mc->conditional_expressions[i];
            
            // Check if varsel_id is within bounds
            int required_size = (info->varsel_id + 1) * (1 << num_total_input_vars);
            if (required_size > mc->vardata_lut_size) {
                fprintf(stderr, "Error: varsel_id %d exceeds allocated vardata_lut size %d\n",
                        info->varsel_id, mc->vardata_lut_size);
                continue; // Skip this entry to prevent buffer overflow
            }
            
            if (info->sim_expr && info->sim_expr->LUT) {
                // Copy the LUT for this expression into the main vardata_lut
                // The position in vardata_lut is determined by the varsel_id
                // Assuming varsel_id maps directly to the block index in vardata_lut
                memcpy(mc->vardata_lut + (info->varsel_id * (1 << num_total_input_vars)),
                       info->sim_expr->LUT,
                       (1 << num_total_input_vars) * sizeof(uint8_t));
            } else {
                fprintf(stderr, "Warning: No LUT found for varsel_id %d. Skipping copy.\n", info->varsel_id);
                // Optionally, fill with zeros or a default value
                memset(mc->vardata_lut + (info->varsel_id * (1 << num_total_input_vars)),
                       0,
                       (1 << num_total_input_vars) * sizeof(uint8_t));
            }
        }
        print_debug("DEBUG: Final vardata_lut content: ");
        for (int i = 0; i < mc->vardata_lut_size; i++) {
            fprintf(stderr, "%d ", mc->vardata_lut[i]);
        }
        fprintf(stderr, "\n");
    }
    
    // Write the vardata_lut to file
    // Determine the output filename based on the input source file name
    //char output_filename[256];
    //snprintf(output_filename, sizeof(output_filename), "examples/simple/simple_vardata.mem"); // Hardcoded for now
    //write_vardata_mem_file(mc, output_filename);
    
    return mc;
}

static void resolve_compact_microcode_jumps(CompactMicrocode* mc) {
    for (int i = 0; i < mc->pending_jump_count; i++) {
        PendingJump jump = mc->pending_jumps[i];
        if (jump.instruction_index < mc->instruction_count) {
            MCode* mcode = &mc->instructions[jump.instruction_index].uword.mcode;
            if (jump.is_exit_jump) {
                mcode->jadr = mc->exit_address;
            } else {
                mcode->jadr = jump.target_instruction_address;
            }
        } else {
            fprintf(stderr, "Warning: Pending jump instruction index out of bounds: %d (max %d)\n",
                    jump.instruction_index, mc->instruction_count - 1);
        }
    }
}

// Helper function to find the end address of a switch using switch info tracking
static int find_switch_end_address(CompactMicrocode* mc, int switch_start_addr) {
    // Find the instruction corresponding to the switch start
    if (switch_start_addr < 0 || switch_start_addr >= mc->instruction_count) {
        return switch_start_addr + 1; // Default fallback
    }
    
    print_debug("DEBUG: find_switch_end_address: Looking for end of switch starting at %d\n", switch_start_addr);
    
    // Look through the switch info we've collected to find the end address for this switch
    for (int i = 0; i < mc->switch_info_count; i++) {
        if (mc->switch_infos[i].switch_start_addr == switch_start_addr) {
            print_debug("DEBUG: find_switch_end_address: Found switch info for start addr %d, end addr %d\n",
                    switch_start_addr, mc->switch_infos[i].switch_end_addr);
            return mc->switch_infos[i].switch_end_addr;
        }
    }
    
    // If we don't have switch info, return a reasonable default
    // This should not happen in a properly functioning system
    print_debug("DEBUG: find_switch_end_address: No switch info found for start addr %d, using fallback\n", switch_start_addr);
    return mc->instruction_count - 1;
}

// Resolve switch break addresses after all instructions have been generated
static void resolve_switch_break_addresses(CompactMicrocode* mc) {
    if (!mc || !mc->pending_switch_breaks || mc->pending_switch_break_count == 0) {
        return;
    }
    
    print_debug("DEBUG: Resolving %d switch break addresses\n", mc->pending_switch_break_count);
    
    // Create a mapping from switch start address to end address
    SwitchInfo switch_infos[MAX_SWITCHES];
    int switch_info_count = 0;
    
    // First, build the switch info mapping
    for (int i = 0; i < mc->pending_switch_break_count; i++) {
        PendingSwitchBreak* pending = &mc->pending_switch_breaks[i];
        int switch_start_addr = pending->switch_start_addr;
        
        // Check if we already have info for this switch
        bool found = false;
        for (int j = 0; j < switch_info_count; j++) {
            if (switch_infos[j].switch_start_addr == switch_start_addr) {
                found = true;
                break;
            }
        }
        
        // If not found, add new switch info
        if (!found && switch_info_count < MAX_SWITCHES) {
            // For now, we'll determine the end address dynamically
            // We'll update this with the actual end address later
            switch_infos[switch_info_count].switch_start_addr = switch_start_addr;
            switch_infos[switch_info_count].switch_end_addr = -1; // Will be determined later
            switch_infos[switch_info_count].first_break_index = i;
            switch_infos[switch_info_count].break_count = 1;
            switch_info_count++;
        } else if (found) {
            // Increment break count for existing switch
            for (int j = 0; j < switch_info_count; j++) {
                if (switch_infos[j].switch_start_addr == switch_start_addr) {
                    switch_infos[j].break_count++;
                    break;
                }
            }
        }
    }
    
    // Build a complete map of all switch boundaries using a stack-based approach
    typedef struct {
        int start_addr;
        int end_addr;
    } SwitchBoundary;
    
    SwitchBoundary boundaries[MAX_SWITCHES];
    int boundary_count = 0;
    
    // Stack to track open switches
    int switch_stack[MAX_SWITCHES];
    int stack_top = -1;
    
    // Scan through all instructions to match switches with their closing "}}"
    for (int i = 0; i < mc->instruction_count; i++) {
        const char* label = mc->instructions[i].label;
        if (label) {
            // Check for SWITCH instruction (not CASE or DEFAULT)
            if (strstr(label, "SWITCH") != NULL &&
                strstr(label, "CASE") == NULL &&
                strstr(label, "DEFAULT") == NULL) {
                // Push this switch onto the stack
                stack_top++;
                if (stack_top < MAX_SWITCHES) {
                    switch_stack[stack_top] = i;
                    print_debug("DEBUG: Found SWITCH at %d (0x%x), stack depth %d\n",
                            i, i, stack_top + 1);
                }
            }
            // Check for "}}" which closes a switch
            else if (strcmp(label, "}}") == 0) {
                if (stack_top >= 0) {
                    // Pop the most recent switch from the stack
                    int start = switch_stack[stack_top];
                    stack_top--;
                    
                    // Record this switch boundary
                    if (boundary_count < MAX_SWITCHES) {
                        boundaries[boundary_count].start_addr = start;
                        boundaries[boundary_count].end_addr = i + 1; // Jump to after "}}"
                        print_debug("DEBUG: Matched SWITCH at %d with }} at %d, breaks jump to %d (0x%x)\n",
                                start, i, i + 1, i + 1);
                        boundary_count++;
                    }
                }
            }
        }
    }
    
    // Now update the switch_infos with the correct end addresses
    for (int i = 0; i < switch_info_count; i++) {
        int switch_start = switch_infos[i].switch_start_addr;
        int switch_end = -1;
        
        // Find the matching boundary
        for (int j = 0; j < boundary_count; j++) {
            if (boundaries[j].start_addr == switch_start) {
                switch_end = boundaries[j].end_addr;
                break;
            }
        }
        
        if (switch_end == -1) {
            // Fallback: use the switch info if available
            switch_end = find_switch_end_address(mc, switch_start);
        }
        
        switch_infos[i].switch_end_addr = switch_end;
        print_debug("DEBUG: Switch info: start=%d, end=%d (0x%x)\n",
                switch_start, switch_end, switch_end);
    }
    
    // Now resolve all break addresses
    // We need to match breaks with their containing switches based on instruction position
    for (int i = 0; i < mc->pending_switch_break_count; i++) {
        PendingSwitchBreak* pending = &mc->pending_switch_breaks[i];
        
        if (pending->instruction_index < mc->instruction_count) {
            int break_addr = pending->instruction_index;
            int target_addr = -1;
            int innermost_start = -1;
            int innermost_end = -1;
            
            // Find the innermost switch that contains this break instruction
            // We need to find the switch with the smallest range that still contains the break
            for (int j = 0; j < boundary_count; j++) {
                if (boundaries[j].start_addr < break_addr &&
                    break_addr < boundaries[j].end_addr - 1) {
                    // This break is inside this switch
                    // Check if this is more inner than what we've found so far
                    if (innermost_start == -1 ||
                        (boundaries[j].end_addr - boundaries[j].start_addr) < (innermost_end - innermost_start)) {
                        innermost_start = boundaries[j].start_addr;
                        innermost_end = boundaries[j].end_addr;
                        target_addr = boundaries[j].end_addr;
                    }
                }
            }
            
            if (target_addr != -1) {
                print_debug("DEBUG: Break at %d belongs to innermost switch %d-%d, jumping to 0x%x\n",
                        break_addr, innermost_start, innermost_end - 1, target_addr);
            }
            
            // Update the jump address
            if (target_addr != -1) {
                mc->instructions[pending->instruction_index].uword.mcode.jadr = target_addr;
                print_debug("DEBUG: Fixed instruction %d to jump to 0x%x\n",
                        pending->instruction_index, target_addr);
            } else {
                fprintf(stderr, "WARNING: Could not find containing switch for break at %d\n", break_addr);
            }
        }
    }
    
    // Reset the pending switch break count for next function
    mc->pending_switch_break_count = 0;
}

// Forward declaration for counting instructions
static int count_statements(Node* stmt);

// Count how many instructions a statement will generate
static int count_statements(Node* stmt) {
    if (!stmt) return 0;
    
    int count = 0; // Initialize count to 0
    // Add a debug print at the very beginning of the function

    switch (stmt->type) {
        case NODE_WHILE: {
            WhileNode* while_node = (WhileNode*)stmt;
            count = 1; // while header
            
            // Count body statements
            if (while_node->body) {
                if (while_node->body->type == NODE_BLOCK) {
                    BlockNode* block = (BlockNode*)while_node->body;
                    if (block->statements) {
                        for (int i = 0; i < block->statements->count; i++) {
                            count += count_statements(block->statements->items[i]);
                        }
                    }
                } else {
                    count += count_statements(while_node->body);
                }
            }
            
            // count++; // closing brace jump (removed to match hotstate instruction count)
            break; // Use break instead of return here
        }
        
        case NODE_IF: {
            IfNode* if_node = (IfNode*)stmt;
            count = 1; // if condition
            
            // Count then body
            if (if_node->then_branch) {
                if (if_node->then_branch->type == NODE_BLOCK) {
                    BlockNode* block = (BlockNode*)if_node->then_branch;
                    if (block->statements) {
                        for (int i = 0; i < block->statements->count; i++) {
                            count += count_statements(block->statements->items[i]);
                        }
                    }
                } else {
                    count += count_statements(if_node->then_branch);
                }
            }
            
            // Count else
            if (if_node->else_branch) {
                count++; // else instruction
                if (if_node->else_branch->type == NODE_BLOCK) {
                    BlockNode* block = (BlockNode*)if_node->else_branch;
                    if (block->statements) {
                        for (int i = 0; i < block->statements->count; i++) {
                            count += count_statements(block->statements->items[i]);
                        }
                    }
                } else {
                    count += count_statements(if_node->else_branch);
                }
            }
            
            break; // Use break instead of return here
        }
        
        case NODE_SWITCH: {
            SwitchNode* switch_node = (SwitchNode*)stmt;
            // Start with 1 for the switch header itself
            int switch_total_count = 1;
            
            // Count case statements
            if (switch_node->cases) {
                print_debug("DEBUG: count_statements: Switch has %d cases\n", switch_node->cases->count);
                for (int i = 0; i < switch_node->cases->count; i++) {
                    CaseNode* case_node = (CaseNode*)switch_node->cases->items[i];
                    // Add 1 for the case/default label itself (CASE_0, CASE_1, etc.)
                    switch_total_count++;
                    
                    // Count case body statements, but handle nested switches specially
                    if (case_node->body) {
                        print_debug("DEBUG: count_statements: Case %d has %d statements\n", i, case_node->body->count);
                        for (int j = 0; j < case_node->body->count; j++) {
                            Node* stmt_node = case_node->body->items[j];
                            int case_stmt_count;
                            
                            // Check if this statement is a nested switch
                            // We need to look deeper since switches might be wrapped in blocks
                            bool is_nested_switch = false;
                            if (stmt_node->type == NODE_SWITCH) {
                                is_nested_switch = true;
                            } else if (stmt_node->type == NODE_BLOCK) {
                                // Check if the block contains a switch
                                BlockNode* block = (BlockNode*)stmt_node;
                                if (block->statements && block->statements->count > 0) {
                                    for (int k = 0; k < block->statements->count; k++) {
                                        if (block->statements->items[k]->type == NODE_SWITCH) {
                                            is_nested_switch = true;
                                            break;
                                        }
                                    }
                                }
                            }
                            
                            if (is_nested_switch) {
                                // For nested switches, only count the switch header instruction
                                // The nested switch will be processed separately and have its own break targets
                                case_stmt_count = 1;
                                print_debug("DEBUG: count_statements: Case %d statement %d contains nested switch, counting as 1\n", i, j);
                            } else {
                                case_stmt_count = count_statements(stmt_node);
                            }
                            
                            switch_total_count += case_stmt_count;
                            print_debug("DEBUG: count_statements: Case %d statement %d contributes %d, total now %d\n", i, j, case_stmt_count, switch_total_count);
                        }
                    }
                }
            }
            // Add 1 for the closing "}}" instruction at the end of the switch
            switch_total_count++;
            print_debug("DEBUG: count_statements: Final switch count is %d\n", switch_total_count);
            
            // Debug print to check the calculated total for NODE_SWITCH
            // Assign the calculated total to the 'count' variable for this statement
            count = switch_total_count;
            break;
        }
        
        case NODE_ASSIGNMENT:
        case NODE_EXPRESSION_STATEMENT:
            count = 1;
            break;
            
        case NODE_BREAK:
        case NODE_CONTINUE:
            count = 1; // Break and continue generate jump instructions
            break;
            
        case NODE_BLOCK: {
            BlockNode* block = (BlockNode*)stmt;
            count = 0;
            if (block->statements) {
                for (int i = 0; i < block->statements->count; i++) {
                    count += count_statements(block->statements->items[i]);
                }
            }
            // Count for the closing brace '}'
            // Hotstate implicitly generates an instruction for the closing brace of a block
            count++;
            break; // Use break instead of return here
        }
        case NODE_FOR: {
            ForNode* for_node = (ForNode*)stmt;
            count = 0;
            // Initializer
            if (for_node->init) {
                count += count_statements(for_node->init);
            }
            // Condition (at least one instruction for the branch)
            count += 1; // For the condition evaluation and branch instruction
            // Body
            if (for_node->body) {
                count += count_statements(for_node->body);
            }
            // Update
            if (for_node->update) {
                count += count_statements(for_node->update);
            }
            // Jump back to header
            count += 1; // For the unconditional jump back to the loop header
            break; // Use break instead of return here
        }
        default:
            count = 1; // Default to 1 instruction
            break; // Use break instead of return here
    }
    return count;
}

static void process_function(CompactMicrocode* mc, FunctionDefNode* func) {
    int current_addr = 0;
    int* addr = &current_addr;
    
    // Calculate exit address dynamically. This will be the address of the :exit instruction.
    // It is the current address after all statements in the function body are processed.
    // Store exit address for use in jump resolution.
    
    // Add function entry, calculating initial state from hw_ctx
    int initial_state = 0;
    int initial_mask = 0;
    if (mc->hw_ctx) {
        for (int i = 0; i < mc->hw_ctx->state_count; i++) {
            StateVariable* s = &mc->hw_ctx->states[i];
            if (s->initial_value) {
                initial_state |= (1 << s->state_number);
            }
            initial_mask |= (1 << s->state_number);
        }
    }
    
    // Use calculated state and mask. Mask should be 7 for 3 state variables.
    MCode entry_mcode;
    populate_mcode_instruction(mc, &entry_mcode, mc->hw_ctx->initial_state_value, mc->hw_ctx->initial_mask_value, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0); // Use dynamically calculated initial state and mask
    add_compact_instruction(mc, &entry_mcode, "main(){", JUMP_TYPE_DIRECT, 1);
    (*addr)++; // Increment address for main(){
    
    
    if (func->body && func->body->type == NODE_BLOCK) {
        BlockNode* block = (BlockNode*)func->body;
        for (int i = 0; i < block->statements->count; i++) {
            process_statement(mc, block->statements->items[i], addr);
        }
    }
    
    // Set the exit address after all instructions have been generated
    // The exit instruction will be the last instruction, so its address is mc->instruction_count - 1
    mc->exit_address = mc->instruction_count; // Set exit address to the total number of instructions generated

    // Add exit instruction (will be a self-loop after resolution)
    MCode exit_mcode;
    populate_mcode_instruction(mc, &exit_mcode, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0); // jadr placeholder
    add_compact_instruction(mc, &exit_mcode, ":exit", JUMP_TYPE_EXIT, mc->exit_address);
}


// In ast_to_microcode.c (within process_statement function)
static void process_statement(CompactMicrocode* mc, Node* stmt, int* addr) {
    if (!stmt) return;
    
    switch (stmt->type) {
        case NODE_WHILE: {
            WhileNode* while_node = (WhileNode*)stmt;
            
            // Determine continue_target (address of the while loop header) and break_target (address after the loop)
            int while_loop_start_addr = *addr; 
            // Determine break_target (address after the entire while loop)
            int estimated_break_target;
            // Check if it's a while(1) loop
            bool is_while_one = (while_node->condition->type == NODE_NUMBER_LITERAL &&
                                 atoi(((NumberLiteralNode*)while_node->condition)->value) == 1);
            if (while_node->condition->type == NODE_NUMBER_LITERAL) {
            }

            if (is_while_one) {
                estimated_break_target = mc->exit_address; // For while(1), break target is program exit
            } else {
                estimated_break_target = *addr + count_statements((Node*)while_node);
            }

            // Create and push the loop context onto the stack
            LoopSwitchContext current_loop_context = {
                .loop_type = NODE_WHILE,
                .continue_target = while_loop_start_addr,
                .break_target = mc->exit_address // For while(1) this means jump to exit
            };
            push_context(mc, &current_loop_context);
            char* condition_lable_str = NULL;
            char while_full_label[256];
            if (while_node->condition) {
                // process_expression(mc, while_node->condition, addr); // Condition handled directly by while instruction
                condition_lable_str = create_condition_label(while_node->condition);
            } else {
                condition_lable_str = strdup("true"); // Directly set label for while(1)
            }
            snprintf(while_full_label, sizeof(while_full_label), "while (%s) {", condition_lable_str);
            
            
            // Generate the while loop header instruction (jumps to loop_exit_addr if condition is false)
            MCode while_mcode;
            int current_varsel_id = get_hybrid_varsel(while_node->condition, mc);
            populate_mcode_instruction(mc, &while_mcode, 0, 0, 0, current_varsel_id, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0); // jadr placeholder, branch=1, state_capture=1, forced_jmp=0
            add_compact_instruction(mc, &while_mcode, while_full_label, JUMP_TYPE_EXIT, mc->exit_address);
            (*addr)++;

            // Only add conditional expression for complex expressions (varSel > 0) and non-constant conditions
            if (current_varsel_id > 0 && !is_constant_condition(while_node->condition)) {
                add_conditional_expression(mc, while_node->condition, current_varsel_id);
            }
            
            free(condition_lable_str);
            // Process while body statements
            if (while_node->body) {
                if (while_node->body->type == NODE_BLOCK) {
                    BlockNode* block = (BlockNode*)while_node->body;
                    if (block->statements) {
                        for (int i = 0; i < block->statements->count; i++) {
                            process_statement(mc, block->statements->items[i], addr);
                        }
                    }
                } else {
                    process_statement(mc, while_node->body, addr);
                }
            }
            
            // Add the jump back to the loop header for the next iteration
            MCode jump_mcode;
            populate_mcode_instruction(mc, &jump_mcode, 0, 0, current_loop_context.continue_target, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0); // Jadr still needs to be populated for the MCode struct
            add_compact_instruction(mc, &jump_mcode, "}", JUMP_TYPE_CONTINUE, mc->stack_ptr - 1);
            mc->jump_instructions++;
            (*addr)++;
            
            // Pop the context from the stack
            pop_context(mc);
            break;
        }
        
        case NODE_FOR: {
            ForNode* for_node = (ForNode*)stmt;
            // Increment timer_count for each for loop encountered
            mc->timer_count++;
            process_for_loop(mc, for_node, addr); // Delegate to the new for loop processing function
            break;
        }
        
        case NODE_IF: {
            IfNode* if_node = (IfNode*)stmt;
            
            // Determine the condition and create appropriate label
            char* condition_label = create_condition_label(if_node->condition);
            
            // Calculate jump address based on statement structure
            int jump_addr = calculate_jump_address(mc, if_node, *addr);
            
            MCode if_mcode;
            int current_varsel_id;

            // Hybrid approach: simple variables = 0, complex expressions = incremental
            current_varsel_id = get_hybrid_varsel(if_node->condition, mc);

            // Only add conditional expression for complex expressions (varSel > 0) and non-constant conditions
            if (current_varsel_id > 0 && !is_constant_condition(if_node->condition)) {
                add_conditional_expression(mc, if_node->condition, current_varsel_id);
            }
            
            char if_full_label[256];
            snprintf(if_full_label, sizeof(if_full_label), "if (%s) {", condition_label);
            populate_mcode_instruction(mc, &if_mcode, 0, 0, jump_addr, current_varsel_id, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0);
            add_compact_instruction(mc, &if_mcode, if_full_label, JUMP_TYPE_DIRECT, calculate_jump_address(mc, if_node, *addr));
            mc->branch_instructions++;
            (*addr)++;
            
            // Process then branch
            if (if_node->then_branch) {
                process_statement(mc, if_node->then_branch, addr);
            }
            
            // Process else branch if present
            if (if_node->else_branch) {
                // Always generate an "else" instruction first, regardless of whether it's else-if or else-block
                // Calculate the jump address for the else (should jump past the entire if-else chain)
                int else_jump_addr = calculate_else_jump_address(mc, if_node, *addr);
                MCode else_mcode;
                populate_mcode_instruction(mc, &else_mcode, 0, 0, else_jump_addr, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0);
                add_compact_instruction(mc, &else_mcode, "else", JUMP_TYPE_DIRECT, calculate_else_jump_address(mc, if_node, *addr));
                mc->jump_instructions++;
                (*addr)++;
                
                // Then process the else branch (whether it's else-if or else-block)
                process_statement(mc, if_node->else_branch, addr);
            }
            
            free(condition_label);
            break;
        }
        
        case NODE_ASSIGNMENT: {
            AssignmentNode* assign = (AssignmentNode*)stmt;
            process_assignment(mc, assign, addr);
            break;
        }
        
        case NODE_EXPRESSION_STATEMENT: {
            ExpressionStatementNode* expr_stmt = (ExpressionStatementNode*)stmt;
            process_expression_statement(mc, expr_stmt, addr);
            break;
        }
        
        case NODE_BLOCK: {
            BlockNode* block = (BlockNode*)stmt;
            for (int i = 0; i < block->statements->count; i++) {
                process_statement(mc, block->statements->items[i], addr);
            }
            break;
        }
        
        case NODE_SWITCH: {
            SwitchNode* switch_node = (SwitchNode*)stmt;
            print_debug("DEBUG: process_statement: Processing switch at address %d\n", *addr);
            
            // Calculate break target using count_statements
            int estimated_break_target;
            LoopSwitchContext current_switch_context = {
                .loop_type = NODE_SWITCH, // Indicate it's a switch
                .continue_target = -1,    // Continue is not applicable for switch
                .break_target = *addr + count_statements((Node*)switch_node)
            };
            estimated_break_target = current_switch_context.break_target;
            print_debug("DEBUG: process_statement: Switch break target calculated as %d\n", estimated_break_target);
            
            push_context(mc, &current_switch_context);
            
            // Store the current pending switch break count to identify which breaks belong to this switch
            int start_pending_break_count = mc->pending_switch_break_count;
            
            process_switch_statement(mc, switch_node, addr);
            
            // The break target should now be the current address after processing the switch
            print_debug("DEBUG: process_statement: After processing switch, address is %d\n", *addr);
            
            // DO NOT update the break target here - it was already set correctly in process_switch_statement
            // when we knew where the "}}" instruction was. Updating it here would use the wrong address
            // after nested switches have been processed.
            // mc->loop_switch_stack[mc->stack_ptr - 1].break_target = *addr;
            print_debug("DEBUG: process_statement: NOT updating switch break target (already set correctly)\n");
            
            // DO NOT update pending switch breaks here - they already have the correct target
            // from when the switch info was created in process_switch_statement
            for (int i = start_pending_break_count; i < mc->pending_switch_break_count; i++) {
                print_debug("DEBUG: Pending switch break %d at instruction %d already has correct target\n",
                        i, mc->pending_switch_breaks[i].instruction_index);
            }
            
            pop_context(mc); // Pop the switch context after processing
            break;
        }
        
        case NODE_BREAK: {
            // Peek the current loop/switch context
            LoopSwitchContext current_context = peek_context(mc, CONTEXT_TYPE_LOOP_OR_SWITCH);
            if (current_context.break_target == -1) { // Error handling for invalid context
                fprintf(stderr, "Error: 'break' statement used outside of a loop or switch context.\n");
                return;
            }
            // Optional: Use loop_type for more specific validation or behavior
            // The new peek_context handles this, but keeping the warning for additional checks if needed.
            // if (current_context.loop_type != NODE_WHILE && current_context.loop_type != NODE_FOR && current_context.loop_type != NODE_SWITCH) {
            //     fprintf(stderr, "Warning: 'break' used outside of a loop or switch context (type: %d).\n", current_context.loop_type);
            // }

            // For switch statements, we need to store the actual break target address
            // The issue is that the context stack might be popped before resolution
            int jump_target = current_context.break_target;
            
            // Generate a jump instruction to the break_target
            MCode break_mcode;
            populate_mcode_instruction(mc, &break_mcode, 0, 0, jump_target, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0);
            add_compact_instruction(mc, &break_mcode, "break;", JUMP_TYPE_BREAK, mc->stack_ptr - 1);
            
            // If this is a switch break, add it to the pending list for later resolution
            printf("DEBUG: Processing break statement at instruction index %d, loop_type=%d, jump_target=%d\n",
                   mc->instruction_count - 1, current_context.loop_type, jump_target);
            if (current_context.loop_type == NODE_SWITCH) {
                if (mc->pending_switch_break_count < MAX_PENDING_SWITCH_BREAKS) {
                    mc->pending_switch_breaks[mc->pending_switch_break_count].instruction_index = mc->instruction_count - 1;
                    // Store the context stack index so we can find the correct break target later
                    // We need to find the switch context on the stack
                    int switch_context_index = -1;
                    int switch_start_addr = -1;
                    for (int i = mc->stack_ptr - 1; i >= 0; i--) {
                        if (mc->loop_switch_stack[i].loop_type == NODE_SWITCH) {
                            switch_context_index = i;
                            switch_start_addr = mc->loop_switch_stack[i].continue_target; // Use continue_target as switch start
                            break;
                        }
                    }
                    mc->pending_switch_breaks[mc->pending_switch_break_count].switch_start_addr = switch_start_addr;
                    printf("DEBUG: Added pending switch break %d with instruction index %d (switch_start_addr=%d)\n",
                           mc->pending_switch_break_count, mc->instruction_count - 1, switch_start_addr);
                    mc->pending_switch_break_count++;
                } else {
                    fprintf(stderr, "Error: Too many pending switch breaks (max %d)\n", MAX_PENDING_SWITCH_BREAKS);
                }
            } else {
                print_debug("DEBUG: Break statement at instruction %d not added to pending list (loop_type=%d)\n", mc->instruction_count - 1, current_context.loop_type);
            }
            
            mc->jump_instructions++;
            (*addr)++;
            // pop_context(mc); // Removed - Context should not be popped by individual break statements
            break;
        }
        
        case NODE_CONTINUE: {
            LoopSwitchContext current_context = peek_context(mc, CONTEXT_TYPE_LOOP);
            if (current_context.break_target == -1) { // Error handling for invalid context
                fprintf(stderr, "Error: 'continue' statement used outside of a loop context.\n");
                return;
            }
            // Optional: Use loop_type for more specific validation or behavior
            // The new peek_context handles this, but keeping the warning for additional checks if needed.
            // if (current_context.loop_type != NODE_WHILE && current_context.loop_type != NODE_FOR) {
            //     fprintf(stderr, "Warning: 'continue' used outside of a loop context (type: %d).\n", current_context.loop_type);
            // }

            // Generate a jump instruction to the continue_target
            MCode continue_mcode;
            populate_mcode_instruction(mc, &continue_mcode, 0, 0, current_context.continue_target, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0);
            add_compact_instruction(mc, &continue_mcode, "continue;", JUMP_TYPE_CONTINUE, mc->stack_ptr - 1);
            mc->jump_instructions++;
            (*addr)++;
            // pop_context(mc); // Removed - Context should not be popped by individual continue statements
            break;
        }
        
        default:
            // Skip other statement types for now
            break;
    }
}

// Process switch statement and generate microcode with hotstate-compatible switch memory
static void process_switch_statement(CompactMicrocode* mc, SwitchNode* switch_node, int* addr) {
    if (!switch_node || !switch_node->expression) {
        return;
    }
    
    print_debug("DEBUG: process_switch_statement: Starting switch at address %d\n", *addr);
    
    // Assign unique switch ID
    int switch_id = mc->switch_count++;
    if (switch_id >= MAX_SWITCHES) {
        printf("Error: Too many switches (max %d)\n", MAX_SWITCHES);
        return;
    }
    
    int switch_expression_input_num = 0; // Default to 0 or an error value
    if (switch_node->expression->type == NODE_IDENTIFIER) {
        IdentifierNode* ident = (IdentifierNode*)switch_node->expression;
        switch_expression_input_num = get_input_number_by_name(mc->hw_ctx, ident->name);
        // Handle case where input_number is -1 (not found) if necessary
    } else {
        // Handle other expression types if needed, or error out
        fprintf(stderr, "Error: Switch expression is not an identifier. Type: %d\n", switch_node->expression->type);
        return;
    }

    // Calculate the estimated break_target for this switch statement
    // This is the address immediately after the entire switch block
    int estimated_break_target = *addr + count_statements((Node*)switch_node);
    print_debug("DEBUG: process_switch_statement: Switch size calculated as %d\n", count_statements((Node*)switch_node));
    print_debug("DEBUG: process_switch_statement: Estimated break target: %d\n", estimated_break_target);

    // Push the current switch context onto the stack
    LoopSwitchContext current_switch_context = {
        .loop_type = NODE_SWITCH, // Indicate it's a switch
        .continue_target = *addr, // Store switch start address for break resolution
        .break_target = estimated_break_target
    };
    push_context(mc, &current_switch_context);

    // Generate switch instruction with proper hotstate fields
    MCode switch_mcode;
    int current_varsel_id;

    // Hybrid approach: simple variables = 0, complex expressions = incremental
    current_varsel_id = get_hybrid_varsel(switch_node->expression, mc);

    // Only add conditional expression for complex expressions (varSel > 0) and non-constant conditions
    if (current_varsel_id > 0 && !is_constant_condition(switch_node->expression)) {
        add_conditional_expression(mc, switch_node->expression, current_varsel_id);
    }
    
    // Pass switch_expression_input_num as switch_adr
    populate_mcode_instruction(mc, &switch_mcode, 0, 0, 0, current_varsel_id, 0, 0, switch_expression_input_num, 1, 0, 0, 0, 0, 0, 0);
    
    // Create dynamic label that includes the variable name
    char switch_label[128];
    if (switch_node->expression->type == NODE_IDENTIFIER) {
        IdentifierNode* ident = (IdentifierNode*)switch_node->expression;
        snprintf(switch_label, sizeof(switch_label), "SWITCH (%s)", ident->name);
    } else {
        snprintf(switch_label, sizeof(switch_label), "SWITCH (expr)");
    }
    
    add_switch_instruction(mc, &switch_mcode, switch_label, switch_id);
    (*addr)++;
    
    // Populate switch memory table
    populate_switch_memory(mc, switch_id, switch_node, addr);
    
    // Process case bodies (generate actual case target instructions)
    int num_cases = switch_node->cases ? switch_node->cases->count : 0;
    print_debug("DEBUG: process_switch_statement: Processing %d cases\n", num_cases);
    for (int i = 0; i < num_cases; i++) {
        CaseNode* case_node = (CaseNode*)switch_node->cases->items[i];
        
        // Mark this address as a case target
        char case_label[64];
        if (case_node->value && case_node->value->type == NODE_NUMBER_LITERAL) {
            NumberLiteralNode* num = (NumberLiteralNode*)case_node->value;
            snprintf(case_label, sizeof(case_label), "CASE_%s", num->value);
        } else {
            snprintf(case_label, sizeof(case_label), "DEFAULT_CASE");
        }
        
        // Generate case target instruction (normal instruction, not switch)
        MCode case_mcode;
        populate_mcode_instruction(mc, &case_mcode, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); // switch_sel should be 0 for CASE
        add_case_instruction(mc, &case_mcode, case_label, switch_id);
        // (*addr)++; // Removed to not increment address for CASE labels
        
        // Process case statements
        if (case_node->body) {
            print_debug("DEBUG: process_switch_statement: Case %d has %d body statements\n", i, case_node->body->count);
            for (int j = 0; j < case_node->body->count; j++) {
                process_statement(mc, case_node->body->items[j], addr);
            }
        }
        // Pop the switch context from the stack
    }
    
    // Note: Default case is handled as a CaseNode with value=NULL in the cases list above
    MCode end_switch_mcode;
    populate_mcode_instruction(mc, &end_switch_mcode, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    
    // Store the address where the "}}" will be placed
    int switch_closing_addr = *addr;
    
    add_compact_instruction(mc, &end_switch_mcode, "}}", JUMP_TYPE_DIRECT, 0);
    (*addr)++;
    
    // The break target should be the instruction AFTER the "}}"
    int break_jump_target = *addr;
    
    // Update the break target in the context to the actual final address
    mc->loop_switch_stack[mc->stack_ptr - 1].break_target = break_jump_target;
    print_debug("DEBUG: process_switch_statement: Switch closing at %d, breaks should jump to %d\n",
            switch_closing_addr, break_jump_target);
    
    // TWO-PASS SYSTEM: Store where breaks should jump to (after the "}}")
    int current_switch_start_addr = mc->loop_switch_stack[mc->stack_ptr - 1].continue_target;
    
    // Check if we already have switch info for this switch
    bool found = false;
    for (int i = 0; i < mc->switch_info_count; i++) {
        if (mc->switch_infos[i].switch_start_addr == current_switch_start_addr) {
            // Update with the break jump target (instruction after "}}")
            mc->switch_infos[i].switch_end_addr = break_jump_target;
            print_debug("DEBUG: process_switch_statement: Updated switch info for start addr %d, break target %d\n",
                    current_switch_start_addr, break_jump_target);
            found = true;
            break;
        }
    }
    
    // If not found, create new switch info with break jump target
    if (!found && mc->switch_info_count < MAX_SWITCHES) {
        mc->switch_infos[mc->switch_info_count].switch_start_addr = current_switch_start_addr;
        mc->switch_infos[mc->switch_info_count].switch_end_addr = break_jump_target;
        mc->switch_infos[mc->switch_info_count].first_break_index = -1;
        mc->switch_infos[mc->switch_info_count].break_count = 0;
        mc->switch_info_count++;
        print_debug("DEBUG: process_switch_statement: Created switch info for start addr %d, break target %d\n",
                current_switch_start_addr, break_jump_target);
    }
    
    // Pop the switch context from the stack
    pop_context(mc);
}


// Helper function to get input variable number from identifier name
static int get_input_var_number(const char* var_name) {
    // Map common input variable names to input numbers
    // This is a simplified mapping - in a real implementation, this would
    // come from symbol table or hardware context
    if (strcmp(var_name, "case_in") == 0) return 0;
    if (strcmp(var_name, "new_case") == 0) return 1;
    if (strncmp(var_name, "input", 5) == 0) return 2; // input0, input1, etc.
    return -1; // Not an input variable
}

// Process expression and generate microcode
static void process_expression(CompactMicrocode* mc, Node* expr, int* addr) {
    if (!expr) return;
    
    switch (expr->type) {
        case NODE_IDENTIFIER: {
            IdentifierNode* ident = (IdentifierNode*)expr;
            
            // Check if this is an input variable and get its number
            int input_num = get_input_var_number(ident->name);
            
            // Generate load instruction for identifier
            // For load instructions, varSel should be 0 (non-conditional)
            MCode load_mcode;
            populate_mcode_instruction(mc, &load_mcode, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            
            char label[64];
            snprintf(label, sizeof(label), "load %s", ident->name);
            add_compact_instruction(mc, &load_mcode, label, JUMP_TYPE_DIRECT, 0);
            (*addr)++;
            break;
        }
        
        case NODE_NUMBER_LITERAL: {
            NumberLiteralNode* num = (NumberLiteralNode*)expr;
            // Generate immediate load instruction
            MCode imm_mcode;
            populate_mcode_instruction(mc, &imm_mcode, 0, 2, 0, 0, 0, 0, 0, atoi(num->value), 0, 0, 0, 0, 0, 0);
            char label[64];
            snprintf(label, sizeof(label), "load #%s", num->value);
            add_compact_instruction(mc, &imm_mcode, label, JUMP_TYPE_DIRECT, 0);
            (*addr)++;
            break;
        }
        
        case NODE_BINARY_OP: {
            BinaryOpNode* binop = (BinaryOpNode*)expr;
            // Process left operand
            process_expression(mc, binop->left, addr);
            // Process right operand
            process_expression(mc, binop->right, addr);
            // Generate operation instruction
            MCode op_mcode;
            populate_mcode_instruction(mc, &op_mcode, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            add_compact_instruction(mc, &op_mcode, "binop", JUMP_TYPE_DIRECT, 0);
            (*addr)++;
            break;
        }
        
        default:
            // Skip other expression types for now
            break;
    }
}

static void add_compact_instruction(CompactMicrocode* mc, MCode* mcode, const char* label, JumpType jump_type, int jump_target_param) {
    if (mc->instruction_count >= mc->instruction_capacity) {
        mc->instruction_capacity *= 2;
        mc->instructions = (Code*)realloc(mc->instructions, sizeof(mc->instructions[0]) * mc->instruction_capacity);
    }

    mc->instructions[mc->instruction_count].uword.mcode = *mcode;
    mc->instructions[mc->instruction_count].label = label ? strdup(label) : NULL;

    // If this is a jump instruction, add it to pending_jumps
    if (mcode->branch || mcode->forced_jmp) {
        int resolved_jadr = 0;
        bool is_exit_target_jump = false;

        if (jump_type == JUMP_TYPE_BREAK) {
            LoopSwitchContext ctx = peek_context(mc, CONTEXT_TYPE_LOOP_OR_SWITCH);
            resolved_jadr = ctx.break_target;
            print_debug("DEBUG: add_compact_instruction: BREAK instruction at index %d, resolved_jadr=%d\n", mc->instruction_count, resolved_jadr);
        } else if (jump_type == JUMP_TYPE_CONTINUE) {
            LoopSwitchContext ctx = peek_context(mc, CONTEXT_TYPE_LOOP);
            resolved_jadr = ctx.continue_target;
        } else if (jump_type == JUMP_TYPE_DIRECT) {
            resolved_jadr = jump_target_param;
        } else if (jump_type == JUMP_TYPE_EXIT) {
            resolved_jadr = mc->exit_address;
            is_exit_target_jump = true;
        }
        add_pending_jump(mc, mc->instruction_count, resolved_jadr, is_exit_target_jump);
    }
    mc->instruction_count++;
}


static char* create_condition_label(Node* condition) {
    char buffer[256]; // Use a stack-allocated buffer for intermediate string building
    buffer[0] = '\0'; // Initialize buffer

    if (!condition) {
        return strdup("true"); // Default for empty condition (e.g., while(1))
    }

    switch (condition->type) {
        case NODE_BINARY_OP: {
            BinaryOpNode* binop = (BinaryOpNode*)condition;
            char* left_str = create_condition_label(binop->left);
            char* right_str = create_condition_label(binop->right);

            const char* op_str;
            switch (binop->op) {
                case TOKEN_AND: op_str = "&&"; break;
                case TOKEN_OR:  op_str = "||"; break;
                case TOKEN_EQUAL: op_str = "=="; break;
                case TOKEN_NOT_EQUAL: op_str = "!="; break;
                case TOKEN_LESS:  op_str = "<";  break;
                case TOKEN_LESS_EQUAL:  op_str = "<="; break;
                case TOKEN_GREATER:  op_str = ">";  break;
                case TOKEN_GREATER_EQUAL:op_str = ">="; break;
                case TOKEN_LOGICAL_AND: op_str = "&&"; break; // Added for completeness
                case TOKEN_LOGICAL_OR: op_str = "||"; break;  // Added for completeness
                default: op_str = "??"; break;
            }
            snprintf(buffer, sizeof(buffer), "(%s %s %s)", left_str, op_str, right_str);
            free(left_str);
            free(right_str);
            break;
        }
        case NODE_UNARY_OP: {
            UnaryOpNode* unop = (UnaryOpNode*)condition;
            char* operand_str = create_condition_label(unop->operand);
            const char* op_str;
            switch (unop->op) {
                case TOKEN_NOT: op_str = "!"; break;
                default: op_str = "?"; break;
            }
            snprintf(buffer, sizeof(buffer), "%s(%s)", op_str, operand_str);
            free(operand_str);
            break;
        }
        case NODE_IDENTIFIER: {
            IdentifierNode* ident = (IdentifierNode*)condition;
            strncpy(buffer, ident->name, sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0'; // Ensure null-termination
            break;
        }
        case NODE_NUMBER_LITERAL: {
            NumberLiteralNode* num = (NumberLiteralNode*)condition;
            strncpy(buffer, num->value, sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0'; // Ensure null-termination
            break;
        }
        case NODE_ARRAY_ACCESS: { // Added for array access in conditions (e.g., a[0])
            ArrayAccessNode* arr_access = (ArrayAccessNode*)condition;
            char* array_name = create_condition_label(arr_access->array);
            char* index_str = create_condition_label(arr_access->index);
            snprintf(buffer, sizeof(buffer), "%s[%s]", array_name, index_str);
            free(array_name);
            free(index_str);
            break;
        }
        default:
            snprintf(buffer, sizeof(buffer), "unhandled_condition_type_%d", condition->type);
            break;
    }
    
    char* result_str = strdup(buffer); // Allocate memory for the final string to be returned
    if (!result_str) {
        perror("Failed to allocate memory for condition string");
        return strdup("error");
    }
    return result_str;
}

static int calculate_jump_address(CompactMicrocode* mc, IfNode* if_node, int current_addr) {
    // For branch instructions, calculate where to jump if condition is FALSE
    // This should jump to the next statement after the if-then-else block
    
    // Estimate the size of the then branch
    int then_size = estimate_statement_size(if_node->then_branch);
    
    int jump_target;
    if (if_node->else_branch) {
        // If there's an else branch, jump to the else part
        jump_target = current_addr + 1 + then_size + 1; // +1 for if, +then_size, +1 for else
    } else {
        // If no else branch, jump past the then branch
        jump_target = current_addr + 1 + then_size;
    }
    print_debug("DEBUG: calculate_jump_address: current_addr=%d, then_size=%d, else_branch=%p, jump_target=%d\n",
            current_addr, then_size, (void*)if_node->else_branch, jump_target);
    return jump_target;
}

static int calculate_else_jump_address(CompactMicrocode* mc, IfNode* if_node, int current_addr) {
    // For else instructions, calculate where to jump (past the entire if-else block)
    int else_size = estimate_statement_size(if_node->else_branch);
    int jump_target = current_addr + 1 + else_size; // +1 for else instruction, +else_size
    print_debug("DEBUG: calculate_else_jump_address: current_addr=%d, else_size=%d, jump_target=%d\n",
            current_addr, else_size, jump_target);
    return jump_target;
}

// Helper function to estimate how many instructions a statement will generate
static int estimate_statement_size(Node* stmt) {
    if (!stmt) {
        print_debug("DEBUG: estimate_statement_size: NULL stmt, returning 0\n");
        return 0;
    }
    
    int size = 0;
    switch (stmt->type) {
        case NODE_ASSIGNMENT:
            size = 1;
            break;
        case NODE_EXPRESSION_STATEMENT: // This handles comma expressions
            size = 1;
            break;
        case NODE_BLOCK: {
            BlockNode* block = (BlockNode*)stmt;
            for (int i = 0; i < block->statements->count; i++) {
                size += estimate_statement_size(block->statements->items[i]);
            }
            break;
        }
        case NODE_IF: {
            IfNode* if_node = (IfNode*)stmt;
            size = 1; // for the if instruction itself
            // Hotstate seems to flatten nested ifs for size estimation
            // It appears to count 1 for the 'then' branch, and 1 for the 'else' instruction.
            // This is a heuristic to match hotstate's output for nested if-else if.
            if (if_node->then_branch) {
                // If the then_branch is a block, we need to count its statements
                if (if_node->then_branch->type == NODE_BLOCK) {
                    BlockNode* block = (BlockNode*)if_node->then_branch;
                    for (int i = 0; i < block->statements->count; i++) {
                        size += estimate_statement_size(block->statements->items[i]);
                    }
                } else {
                    size += 1; // For a single statement in then branch
                }
            } else {
                // If then_branch is NULL, it still implicitly consumes an instruction for fall-through.
                // This accounts for cases like 'if (cond) ; else { ... }'
                size += 1;
            }
            if (if_node->else_branch) {
                size += 1; // for else instruction
                // If the else_branch is a block, count its statements
                if (if_node->else_branch->type == NODE_BLOCK) {
                    BlockNode* block = (BlockNode*)if_node->else_branch;
                    for (int i = 0; i < block->statements->count; i++) {
                        size += estimate_statement_size(block->statements->items[i]);
                    }
                } else {
                    size += 1; // For a single statement in else branch
                }
            }
            size += 1; // for the closing "}}" instruction
            break;
        }
        case NODE_WHILE: {
            WhileNode* while_node = (WhileNode*)stmt;
            size = 1; // for the while instruction itself
            size += estimate_statement_size(while_node->body);
            size += 1; // for the jump back to header
            break;
        }
        case NODE_FOR: {
            ForNode* for_node = (ForNode*)stmt;
            size = 0; // Initializer and update are part of loop structure, not separate instructions for size estimate
            if (for_node->init) {
                size += estimate_statement_size(for_node->init);
            }
            size += 1; // Condition evaluation and branch
            size += estimate_statement_size(for_node->body);
            if (for_node->update) {
                size += estimate_statement_size(for_node->update);
            }
            size += 1; // Jump back to header
            break;
        }
        case NODE_BREAK:
        case NODE_CONTINUE:
            size = 1; // Break and continue generate jump instructions
            break;
        case NODE_FUNCTION_CALL: // Like led0=1,led0=0 but could be a function call
            size = 1;
            break;
        default:
            size = 1; // conservative estimate for unhandled types
            break;
    }
    print_debug("DEBUG: estimate_statement_size: Node type %d, size=%d\n", stmt->type, size);
    return size;
}

static void process_assignment(CompactMicrocode* mc, AssignmentNode* assign, int* addr) {
    if (assign->identifier && assign->identifier->type == NODE_IDENTIFIER) {
        IdentifierNode* id = (IdentifierNode*)assign->identifier;
        
        // Calculate hotstate-compatible state and mask fields
        int state_field = 0, mask_field = 0;
        calculate_hotstate_fields(assign, mc->hw_ctx, &state_field, &mask_field);
        print_debug("DEBUG: process_assignment: id=%s, state_field=%d, mask_field=%d\n", id->name, state_field, mask_field);
        
        
        if (mask_field > 0) {  // This is a state variable assignment
            // Get assignment value for label
            int assign_value = 1;  // Default to 1
            if (assign->value && assign->value->type == NODE_NUMBER_LITERAL) {
                NumberLiteralNode* num = (NumberLiteralNode*)assign->value;
                assign_value = atoi(num->value);
            }
            
            // Encode instruction with hotstate-compatible fields
            MCode assign_mcode;
            populate_mcode_instruction(mc, &assign_mcode,
                state_field,  // state field (bit pattern)
                mask_field,   // mask field (mask bit pattern)
                0,           // jadr
                0,           // varSel
                0,           // timerSel
                0,           // timerLd
                0,           // switch_sel
                0,           // switch_adr
                1,           // state_capture
                0,           // var_or_timer
                0,           // branch
                0,           // forced_jmp
                0,           // sub
                0            // rtn
            );
            
            char label[64];
            snprintf(label, sizeof(label), "%s=%d;", id->name, assign_value);
            add_compact_instruction(mc, &assign_mcode, label, JUMP_TYPE_DIRECT, 0);
            mc->state_assignments++;
            (*addr)++;
        }
    }
}


// Helper function to reconstruct source code from AST nodes
static char* reconstruct_source_code(Node* node) {
    if (!node) return strdup("unknown_null");
    
    char* result = malloc(256);
    result[0] = '\0';
    
    switch (node->type) {
        case NODE_BINARY_OP: {
            BinaryOpNode* binop = (BinaryOpNode*)node;
            if (binop->op == TOKEN_COMMA) {
                // Handle comma expressions like "nst[1] = 0, nst[2] = 0"
                char* left_str = reconstruct_source_code(binop->left);
                char* right_str = reconstruct_source_code(binop->right);
                snprintf(result, 256, "%s, %s", left_str, right_str);
                free(left_str);
                free(right_str);
            } else if (binop->op == TOKEN_ASSIGN) {
                // Handle assignments like "nst[1] = 0"
                char* left_str = reconstruct_source_code(binop->left);
                char* right_str = reconstruct_source_code(binop->right);
                snprintf(result, 256, "%s = %s", left_str, right_str);
                free(left_str);
                free(right_str);
            } else {
                snprintf(result, 256, "binary_op_%d", binop->op);
            }
            break;
        }
        case NODE_ASSIGNMENT: {
            AssignmentNode* assign = (AssignmentNode*)node;
            char* left_str = reconstruct_source_code(assign->identifier);
            char* right_str = reconstruct_source_code(assign->value);
            snprintf(result, 256, "%s = %s", left_str, right_str);
            free(left_str);
            free(right_str);
            break;
        }
        case NODE_FUNCTION_CALL: {
            // This might be a comma expression parsed as function call
            // For now, just indicate it's a function call
            snprintf(result, 256, "function_call");
            break;
        }
        case NODE_ARRAY_ACCESS: {
            ArrayAccessNode* arr = (ArrayAccessNode*)node;
            if (arr->array && arr->array->type == NODE_IDENTIFIER &&
                arr->index && arr->index->type == NODE_NUMBER_LITERAL) {
                IdentifierNode* arr_name = (IdentifierNode*)arr->array;
                NumberLiteralNode* index = (NumberLiteralNode*)arr->index;
                snprintf(result, 256, "%s[%s]", arr_name->name, index->value);
            } else {
                snprintf(result, 256, "array_access_complex");
            }
            break;
        }
        case NODE_IDENTIFIER: {
            IdentifierNode* ident = (IdentifierNode*)node;
            snprintf(result, 256, "%s", ident->name);
            break;
        }
        case NODE_NUMBER_LITERAL: {
            NumberLiteralNode* num = (NumberLiteralNode*)node;
            snprintf(result, 256, "%s", num->value);
            break;
        }
        default:
            snprintf(result, 256, "unknown_type_%d", node->type);
            break;
    }
    
    return result;
}

static void process_expression_statement(CompactMicrocode* mc, ExpressionStatementNode* expr_stmt, int* addr) {
    if (expr_stmt->expression && expr_stmt->expression->type == NODE_BINARY_OP) {
        BinaryOpNode* binop = (BinaryOpNode*)expr_stmt->expression;
        if (binop->op == TOKEN_COMMA) {
            // Handle comma expressions - calculate proper hotstate fields
            int state_field = 0, mask_field = 0;
            calculate_comma_expression_fields(expr_stmt->expression, mc->hw_ctx, &state_field, &mask_field);
            
            char* source_code = reconstruct_source_code(expr_stmt->expression);
            
            MCode combo_mcode;
            populate_mcode_instruction(mc, &combo_mcode, state_field, mask_field, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0);
            add_compact_instruction(mc, &combo_mcode, source_code, JUMP_TYPE_DIRECT, 0);
            free(source_code);
            mc->state_assignments++;
            (*addr)++;
        }
    } else if (expr_stmt->expression && expr_stmt->expression->type == NODE_ASSIGNMENT) {
        // Handle single assignments
        AssignmentNode* assign = (AssignmentNode*)expr_stmt->expression;
        process_assignment(mc, assign, addr);
    }
}

/*
static char* create_statement_label(Node* stmt) {
    if (!stmt) return strdup("unknown");
    
    switch (stmt->type) {
        case NODE_IF:
            return strdup("if ((a0 == 0) && (a1 == 1)) {");
        case NODE_WHILE:
            return strdup("while (1) {");
        default:
            return strdup("statement");
    }
}
*/


// Helper function to print state assignments (matching hotstate format)
static void print_state_assignments(CompactMicrocode* mc, FILE* output) {
    fprintf(output, "State assignments\n");
    if (mc->hw_ctx) {
        for (int i = 0; i < mc->hw_ctx->state_count; i++) {
            StateVariable* state = &mc->hw_ctx->states[i];
            fprintf(output, "state %d is %s\n", state->state_number, state->name);
        }
    }
    fprintf(output, "\n");
}

// Helper function to print variable mappings (matching hotstate format)
static void print_variable_mappings(CompactMicrocode* mc, FILE* output) {
    fprintf(output, "Variable inputs\n");
    if (mc->hw_ctx) {
        for (int i = 0; i < mc->hw_ctx->input_count; i++) {
            InputVariable* input = &mc->hw_ctx->inputs[i];
            fprintf(output, "var %d is %s\n", input->input_number, input->name);
        }
    }
    fprintf(output, "\n");
}

// Implementation of print_compact_microcode_table (Hotstate-compatible format)
void print_compact_microcode_table(CompactMicrocode* mc, FILE* output) {
    fprintf(output, "\nState Machine Microcode derived from %s\n\n", mc->function_name);
    int numStates = mc->hw_ctx->state_count;
    int numVarSels = mc->hw_ctx->input_count;
    int numVars = mc->hw_ctx->input_count; //TODO: should numVars and numVarSels be the same?
    int gTimers = mc->timer_count; //TODO possibly timer_count and switch_count should be
                                   // moved to the hw_ctx ?
    int gSwitches = mc->switch_count;


    int statenibs = (numStates > 0) ? (numStates + 3) / 4 : 1;
    int varSel_bits = 0;
    if (numVarSels > 1) {
        unsigned int temp = numVarSels - 1;
        while (temp > 0) {
            temp >>= 1;
            varSel_bits++;
        }
    } else {
        varSel_bits = 1;
    }
    int varSel_nibs = (varSel_bits > 0) ? (varSel_bits + 3) / 4 : 1;

    int timer_bits = 0;
    if (gTimers > 1) {
        unsigned int temp = gTimers - 1;
        while (temp > 0) {
            temp >>= 1;
            timer_bits++;
        }
    } else {
        timer_bits = 1;
    }
    int timer_nibs = (timer_bits > 0) ? (timer_bits + 3) / 4 : 1;

    int switch_bits = 0;
    if (gSwitches > 1) {
        unsigned int temp = gSwitches - 1;
        while (temp > 0) {
            temp >>= 1;
            switch_bits++;
        }
    } else {
        switch_bits = 1;
    }
    int switch_nibs = (switch_bits > 0) ? (switch_bits + 3) / 4 : 1;
    int gAddrnibs = (int)ceil(((log10(mc->instruction_count)/log10(2))) / 4.0);
    if (gAddrnibs == 0) gAddrnibs = 1; // Ensure at least 1 nibble for 0 instructions
    print_debug("DEBUG: print_compact_microcode_table: mc->instruction_count = %d, gAddrnibs = %d\n", mc->instruction_count, gAddrnibs);
    
    // Print the header (matching hotstate format)
    ColumnFormat columns[] = {
        {"address",   gAddrnibs, 1},
        {"state",     statenibs, 1},
        {"mask",      statenibs, 1},
        {"jadr",      gAddrnibs, 1},
        {"varSel",    varSel_nibs, numVars > 0},
        {"timSel",    timer_nibs, gTimers > 0},
        {"timLd",     timer_nibs, gTimers > 0},
        {"switchSel", switch_nibs, gSwitches > 0},
        {"sswitchAdr", 1, gSwitches > 0},
        {"stateCap", 1, 1},
        {"tim/var",  1, numVars > 0 || gTimers > 0},
        {"branch",   1, 1},
        {"forcejmp",  1, 1},
        {"sub",  1, 1},
        {"rtn",  1, 1}
    };
    int num_columns = sizeof(columns) / sizeof(columns[0]);
    int max_header_len = 0;
    for (int i = 0; i < num_columns; i++) {
        int len = strlen(columns[i].header);
        if (len > max_header_len) {
            max_header_len = len;
        }
    }
    // Print header columns (bottom-aligned)
    for (int i = 0; i < max_header_len; i++) {
       for (int j = 0; j < num_columns; j++) {
           
           char header_char = ' ';
           int header_len = strlen(columns[j].header);
           
           // Calculate which character to print (bottom-aligned)
           int char_index = i - (max_header_len - header_len);
           
           if (char_index >= 0 && char_index < header_len) {
               header_char = columns[j].header[char_index];
           }
           
           printf("%-*c ", columns[j].width, header_char);
       }
       printf("\n");
    }

    // print separator "-----...----"
    for (int i = 0; i < num_columns; i++) {
        for (int j = 0; j < columns[i].width + 1; j++) {
            printf("-");
        }
    }
    printf("-\n");

    
    // Print each instruction in hotstate format
    // Debug: Check jadr of instruction 1 before printing loop
    if (mc->instruction_count > 1) {
        MCode* debug_mcode = &mc->instructions[1].uword.mcode;
        print_debug("DEBUG: print_compact_microcode_table: Instruction 1 jadr before print loop: %d\n", debug_mcode->jadr);
    }

    for (int i = 0; i < mc->instruction_count; i++) {
        Code* code_entry = &mc->instructions[i];
        MCode* mcode = &code_entry->uword.mcode;
        
        // Extract fields from MCode struct
        uint32_t state = mcode->state;
        uint32_t mask = mcode->mask;
        uint32_t jadr = mcode->jadr;
        uint32_t varsel = mcode->varSel; // Direct access to varSel field
        uint32_t timer_sel = mcode->timerSel;
        uint32_t timer_ld = mcode->timerLd;
        uint32_t switch_sel = mcode->switch_sel;
        uint32_t switch_adr = mcode->switch_adr;
        uint32_t state_cap = mcode->state_capture;
        uint32_t var_timer = mcode->var_or_timer;
        uint32_t branch = mcode->branch;
        uint32_t forced_jmp = mcode->forced_jmp;
        uint32_t sub = mcode->sub;
        uint32_t rtn = mcode->rtn;
        
        
        // Print in hotstate format: addr state mask jadr varsel unused1 unused2 switchsel switchadr flags
        // Use 'x' for fields that are 0 and should be 'x' in hotstate output, or for fields not directly mapped
        // Print in hotstate format: addr state mask jadr varsel unused1 unused2 switchsel switchadr flags
        // Use 'x' for fields that are 0 and should be 'x' in hotstate output, or for fields not directly mapped
        // Modified to use dynamic widths from ColumnFormat
        for (int col_idx = 0; col_idx < num_columns; col_idx++) {
            /* don't skip inactive columns, instead print 'X' in that column:
            if (!columns[col_idx].active) {
                print_debug("DEBUG: print_compact_microcode_table: Skipping inactive column %d\n", col_idx);
                continue; // Skip inactive columns
            }
            */

            // Determine which microcode field to print based on col_idx
            // and use the width from columns[col_idx].width
            switch (col_idx) {
                case 0: fprintf(output, "%0*X", columns[col_idx].width, i); break; // address
                case 1: fprintf(output, " %0*X", columns[col_idx].width, state); break; // state
                case 2: fprintf(output, " %0*X", columns[col_idx].width, mask); break; // mask
                case 3: fprintf(output, " %0*X", columns[col_idx].width, jadr); break; // jadr
                case 4: fprintf(output, (columns[col_idx].active && mc->has_complex_conditionals) ? " %0*X" : " %*c", columns[col_idx].width, (columns[col_idx].active && mc->has_complex_conditionals) ? varsel : 'X') ; break; // varSel
                case 5: fprintf(output, columns[col_idx].active ? " %0*X" : " %*c", columns[col_idx].width, columns[col_idx].active ? timer_sel : 'X'); break; // timSel
                case 6: fprintf(output, columns[col_idx].active ? " %0*X" : " %*c", columns[col_idx].width, columns[col_idx].active ? timer_ld : 'X'); break; // timLd
                case 7: fprintf(output, columns[col_idx].active ? " %0*X" : " %*c", columns[col_idx].width, columns[col_idx].active ? switch_sel : 'X'); break; // switchSel
                case 8: fprintf(output, columns[col_idx].active ? " %0*X" : " %*c", columns[col_idx].width, columns[col_idx].active ? switch_adr : 'X'); break; // sswitchAdr
                case 9: fprintf(output, " %0*X", columns[col_idx].width, state_cap); break; // stateCap
                case 10: fprintf(output, columns[col_idx].active ? " %0*X" : " %*c", columns[col_idx].width, columns[col_idx].active ? var_timer : 'X'); break; // tim/var
                case 11: fprintf(output, columns[col_idx].active ? " %0*X" : " %*c", columns[col_idx].width, columns[col_idx].active ? branch : 'X'); break; // branch
                case 12: fprintf(output, " %0*X", columns[col_idx].width, forced_jmp); break; // forcejmp
                case 13: fprintf(output, " %0*X", columns[col_idx].width, sub); break; // sub
                case 14: fprintf(output, " %0*X", columns[col_idx].width, rtn); break; // rtn
                default: break;
            }
        }
        // Explicitly print sub and rtn outside the loop to ensure they are always displayed
        fprintf(output, "   %s\n", code_entry->label ? code_entry->label : ""); // Label at the end
    }
    
    fprintf(output, "\n");
    
    // Print state and variable assignments
    print_state_assignments(mc, output);
    print_variable_mappings(mc, output);
}

void print_compact_microcode_analysis(CompactMicrocode* mc, FILE* output) {
    fprintf(output, "\n=== Compact Microcode Analysis ===\n");
    fprintf(output, "Function: %s\n", mc->function_name);
    int hotstate_instruction_count = 0;
    for (int i = 0; i < mc->instruction_count; i++) {
        const char* label = mc->instructions[i].label;
        if (!(strcmp(label, "}}") == 0 || strcmp(label, "}") == 0 || strncmp(label, "CASE_", 5) == 0 || strcmp(label, "DEFAULT_CASE") == 0)) {
            hotstate_instruction_count++;
        }
    }
    fprintf(output, "Total instructions: %d\n", hotstate_instruction_count);
    fprintf(output, "State assignments: %d\n", mc->state_assignments);
    fprintf(output, "Branch instructions: %d\n", mc->branch_instructions);
    fprintf(output, "Jump instructions: %d\n", mc->jump_instructions);
    
    // Print switch information
    if (mc->switch_count > 0) {
        fprintf(output, "\nSwitch Information:\n");
        fprintf(output, "Number of switches: %d\n", mc->switch_count);
        fprintf(output, "Switch offset bits: %d (entries per switch: %d)\n",
                mc->switch_offset_bits, 1 << mc->switch_offset_bits);
        
        // Print switch memory table for each switch
        for (int switch_id = 0; switch_id < mc->switch_count; switch_id++) {
            fprintf(output, "\nSwitch %d memory table:\n", switch_id);
            int base_addr = switch_id * (1 << mc->switch_offset_bits);
            int entries_per_switch = 1 << mc->switch_offset_bits;
            
            // Print first 16 entries (or all if less than 16)
            int max_entries = entries_per_switch > 16 ? 16 : entries_per_switch;
            for (int i = 0; i < max_entries; i++) {
                if (mc->switchmem[base_addr + i] != 0) {  // Only show non-zero entries
                    fprintf(output, "  case %d -> address %d\n", i, mc->switchmem[base_addr + i]);
                }
            }
            if (entries_per_switch > 16) {
                fprintf(output, "  ... (showing first 16 entries only)\n");
            }
        }
    }
    
    if (mc->hw_ctx) {
        fprintf(output, "\nHardware Resources:\n");
        fprintf(output, "State variables: %d\n", mc->hw_ctx->state_count);
        fprintf(output, "Input variables: %d\n", mc->hw_ctx->input_count);
    }
}

void free_compact_microcode(CompactMicrocode* mc) {
    if (!mc) return;
    
    for (int i = 0; i < mc->instruction_count; i++) {
        if (mc->instructions[i].label) {
            // printf("Freeing label %d: %s\n", i, mc->instructions[i].label);
            free(mc->instructions[i].label);
            mc->instructions[i].label = NULL; // Prevent double free
        }
    }
    
    free(mc->instructions);
    free(mc->function_name);
    free(mc->loop_switch_stack); // Free loop_switch_stack
    free(mc->switchmem);  // Free switch memory
    free(mc->pending_jumps); // Free the pending jumps array
    free(mc->pending_switch_breaks); // Free the pending switch breaks array
    free(mc->switch_infos); // Free the switch infos array
    free(mc->conditional_expressions); // Free conditional_expressions
    free(mc->vardata_lut); // Free vardata_lut
    free(mc);
}

// Switch management functions implementation

// Populate switch memory table with case mappings
static void populate_switch_memory(CompactMicrocode* mc, int switch_id, SwitchNode* switch_node, int* addr) {
    int base_addr = switch_id * (1 << mc->switch_offset_bits);
    int num_cases = switch_node->cases ? switch_node->cases->count : 0;
    
    // Initialize all entries to default case address (will be calculated later)
    int default_addr = *addr + num_cases;  // Default comes after all cases
    for (int i = 0; i < (1 << mc->switch_offset_bits); i++) {
        mc->switchmem[base_addr + i] = default_addr;
    }
    
    // Set specific case addresses
    int case_addr = *addr;
    for (int i = 0; i < num_cases; i++) {
        CaseNode* case_node = (CaseNode*)switch_node->cases->items[i];
        
        // Get case value
        int case_value = 0;
        if (case_node->value && case_node->value->type == NODE_NUMBER_LITERAL) {
            NumberLiteralNode* num = (NumberLiteralNode*)case_node->value;
            case_value = atoi(num->value);
        }
        
        // Store case address in switch memory
        if (case_value >= 0 && case_value < (1 << mc->switch_offset_bits)) {
            mc->switchmem[base_addr + case_value] = case_addr;
        }
        
        case_addr++;  // Each case gets one address initially
    }
}

// Add switch instruction with switch metadata
static void add_switch_instruction(CompactMicrocode* mc, MCode* mcode, const char* label, int switch_id) {
    if (mc->instruction_count >= mc->instruction_capacity) {
        mc->instruction_capacity *= 2;
        mc->instructions = (Code*)realloc(mc->instructions, sizeof(mc->instructions[0]) * mc->instruction_capacity);
    }
    
    mc->instructions[mc->instruction_count].uword.mcode = *mcode;
    mc->instructions[mc->instruction_count].label = strdup(label);
    mc->instruction_count++;
}

// Add case target instruction with case metadata
static void add_case_instruction(CompactMicrocode* mc, MCode* mcode, const char* label, int switch_id) {
    // Case instructions are not jumps themselves.
    add_compact_instruction(mc, mcode, label, JUMP_TYPE_DIRECT, 0);
}


// Hotstate compatibility functions implementation

// Get state number for a variable name from hardware context
static int get_state_number_for_variable(const char* var_name, HardwareContext* hw_ctx) {
    if (!var_name || !hw_ctx) return -1;
    
    // Search through state variables
    for (int i = 0; i < hw_ctx->state_count; i++) {
        if (hw_ctx->states[i].name && strcmp(hw_ctx->states[i].name, var_name) == 0) {
            return hw_ctx->states[i].state_number;
        }
    }
    
    return -1;  // Not a state variable
}


// Calculate hotstate-compatible state and mask fields for assignments
static void calculate_hotstate_fields(AssignmentNode* assign, HardwareContext* hw_ctx, int* state_out, int* mask_out) {
    *state_out = 0;
    *mask_out = 0;
    
    if (!assign || !assign->identifier || assign->identifier->type != NODE_IDENTIFIER) {
        return;
    }
    
    IdentifierNode* id = (IdentifierNode*)assign->identifier;
    int state_num = get_state_number_for_variable(id->name, hw_ctx);
    
    if (state_num >= 0) {
        // Get assignment value
        int assign_value = 1;  // Default to 1
        if (assign->value && assign->value->type == NODE_NUMBER_LITERAL) {
            NumberLiteralNode* num = (NumberLiteralNode*)assign->value;
            assign_value = atoi(num->value);
        }
        
        // Use exact hotstate logic from exprList.c lines 280-288
        if (assign_value == 1) {
            *state_out |= (1 << state_num);
            *mask_out |= (1 << state_num);
        } else {
            // For assignment to 0: use hotstate's exact logic
            int statenum = -1;
            statenum ^= (1 << state_num);
            *state_out &= statenum;
            *mask_out |= (1 << state_num);
        }
    }
}

// Calculate hotstate-compatible fields for comma expressions (e.g., "LED0=1,LED0=0")
static void calculate_comma_expression_fields(Node* comma_expr, HardwareContext* hw_ctx, int* state_out, int* mask_out) {
    *state_out = 0;
    *mask_out = 0;
    
    if (!comma_expr || comma_expr->type != NODE_BINARY_OP) {
        return;
    }
    
    BinaryOpNode* binop = (BinaryOpNode*)comma_expr;
    if (binop->op != TOKEN_COMMA) {
        return;
    }
    
    // Process left side first (like hotstate processes expressions sequentially)
    if (binop->left) {
        if (binop->left->type == NODE_ASSIGNMENT) {
            calculate_hotstate_fields((AssignmentNode*)binop->left, hw_ctx, state_out, mask_out);
        } else if (binop->left->type == NODE_BINARY_OP) {
            // Handle nested comma expressions
            calculate_comma_expression_fields(binop->left, hw_ctx, state_out, mask_out);
        }
    }
    
    // Process right side, accumulating the mask bits but applying state changes sequentially
    if (binop->right) {
        if (binop->right->type == NODE_ASSIGNMENT) {
            int right_state = *state_out, right_mask = 0;
            calculate_hotstate_fields((AssignmentNode*)binop->right, hw_ctx, &right_state, &right_mask);
            // Apply the right side state change to current state, accumulate mask
            *state_out = right_state;
            *mask_out |= right_mask;  // Accumulate mask bits
        } else if (binop->right->type == NODE_BINARY_OP) {
            // Handle nested comma expressions
            calculate_comma_expression_fields(binop->right, hw_ctx, state_out, mask_out);
        }
    }
}

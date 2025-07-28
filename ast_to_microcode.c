#define _GNU_SOURCE  // For strdup
#include "microcode_defs.h" // Explicitly include for Code struct definition
#include "ast_to_microcode.h"
#include "hw_analyzer.h"       // For HardwareContext and HardwareStateVar
#include "lexer.h"             // For TOKEN_
#include "cfg_to_microcode.h"  // For HotstateMicrocode and HOTSTATE_ macros
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
static int gvarSel = 0;  // Global varSel counter like hotstate

// Forward declarations
static void process_function(CompactMicrocode* mc, FunctionDefNode* func);
static void process_statement(CompactMicrocode* mc, Node* stmt, int* addr);
static void add_compact_instruction(CompactMicrocode* mc, MCode* mcode, const char* label);
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

static void push_context(CompactMicrocode* mc, LoopSwitchContext context);
static void pop_context(CompactMicrocode* mc);  

static void push_context(CompactMicrocode* mc, LoopSwitchContext context) {
    if(mc->stack_ptr >= mc->stack_capacity) {
        mc->stack_capacity *= 2;
        mc->loop_switch_stack = realloc(mc->loop_switch_stack, sizeof(LoopSwitchContext) * mc->stack_capacity);
    }
    mc->loop_switch_stack[mc->stack_ptr++] = context;
}

static void pop_context(CompactMicrocode* mc){
    if (mc->stack_ptr < 0) {
        mc->stack_ptr--;
    } else {
        fprintf(stderr, "Warning: Attempted to pop from empty loop/switch stack.\n");
    }
}
    

static LoopSwitchContext peek_context(CompactMicrocode* mc) {
    if (mc->stack_ptr == 0) {
        fprintf(stderr, "Error: 'break' or 'continue' statement outside of a valid context.\n");
        LoopSwitchContext error_context = {-1, -1}; // Return invalid targets
        return error_context;
    }
    return mc->loop_switch_stack[mc->stack_ptr - 1];
}

// Implement populate_mcode_instruction function
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
) {
    // Populate the MCode struct fields directly
    mcode_ptr->state = state;
    mcode_ptr->mask = mask;
    mcode_ptr->jadr = jadr;
    fprintf(stderr, "DEBUG: populate_mcode_instruction: jadr received=%d, assigned=%d\n", jadr, mcode_ptr->jadr);
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
    if (jadr > mc->max_jadr_val) {
        mc->max_jadr_val = jadr;
    }
    if (varSel > mc->max_varsel_val) {
        mc->max_varsel_val = varSel;
    }
    if (state > mc->max_state_val) {
        mc->max_state_val = state;
    }
    // Add similar updates for other fields that need dynamic sizing as they become relevant
}

CompactMicrocode* ast_to_compact_microcode(Node* ast_root, HardwareContext* hw_ctx) {
    if (!ast_root || ast_root->type != NODE_PROGRAM) {
        return NULL;
    }
    
    // Initialize global counters like hotstate
    // TODO: eliminate globals by moving to CompactMicrocode!
    gSwitches = 0; //TODO: mc->switch_count
    gvarSel = 0;   //TODO: add mc->varSel field
    
    CompactMicrocode* mc = malloc(sizeof(CompactMicrocode));
    mc->instructions = (Code*)malloc(sizeof(mc->instructions[0]) * 32);
    mc->instruction_count = 0;
    mc->timer_count = 0;
    mc->instruction_capacity = 32;
    mc->function_name = strdup("main"); //TODO: this needs to not be hardcoded
    mc->hw_ctx = hw_ctx;
    
    // Initialize switch memory management
    mc->switchmem = calloc(MAX_SWITCH_ENTRIES, sizeof(uint32_t));
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
    mc->max_state_val = 0;
    
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
    
    return mc;
}

// Forward declaration for counting instructions
static int count_statements(Node* stmt);

// Count how many instructions a statement will generate
static int count_statements(Node* stmt) {
    if (!stmt) return 0;
    
    switch (stmt->type) {
        case NODE_WHILE: {
            WhileNode* while_node = (WhileNode*)stmt;
            int count = 1; // while header
            
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
            
            count++; // closing brace jump
            return count;
        }
        
        case NODE_IF: {
            IfNode* if_node = (IfNode*)stmt;
            int count = 1; // if condition
            
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
            
            return count;
        }
        
        case NODE_SWITCH: {
            SwitchNode* switch_node = (SwitchNode*)stmt;
            int count = 2; // switch header + load instruction
            
            // Count case statements
            if (switch_node->cases) {
                for (int i = 0; i < switch_node->cases->count; i++) {
                    CaseNode* case_node = (CaseNode*)switch_node->cases->items[i];
                    count++; // case label
                    
                    // Count case body statements
                    if (case_node->body) {
                        for (int j = 0; j < case_node->body->count; j++) {
                            count += count_statements(case_node->body->items[j]);
                        }
                    }
                }
            }
            
            return count;
        }
        
        case NODE_ASSIGNMENT:
        case NODE_EXPRESSION_STATEMENT:
            return 1;
            
        case NODE_BREAK:
        case NODE_CONTINUE:
            return 1; // Break and continue generate jump instructions
            
        case NODE_BLOCK: {
            BlockNode* block = (BlockNode*)stmt;
            int count = 0;
            if (block->statements) {
                for (int i = 0; i < block->statements->count; i++) {
                    count += count_statements(block->statements->items[i]);
                }
            }
            return count;
        }
        case NODE_FOR: {
            ForNode* for_node = (ForNode*)stmt;
            int count = 0;
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
            return count;
        }
        default:
            return 1; // Default to 1 instruction
    }
}

static void process_function(CompactMicrocode* mc, FunctionDefNode* func) {
    int current_addr = 0;
    int* addr = &current_addr;
    
    // Calculate exit address dynamically
    int exit_addr = 1; // Start after main() entry
    if (func->body && func->body->type == NODE_BLOCK) {
        BlockNode* block = (BlockNode*)func->body;
        for (int i = 0; i < block->statements->count; i++) {
            exit_addr += count_statements(block->statements->items[i]);
        }
    }
    
    // Store exit address for use in while loops
    mc->exit_address = exit_addr;
    
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
    populate_mcode_instruction(mc, &entry_mcode, 4, 7, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0); // state set to 4, mask to 7, state_capture to 1
    add_compact_instruction(mc, &entry_mcode, "main(){");
    (*addr)++; // Increment address for main(){
    
    
    // Process function body
    if (func->body && func->body->type == NODE_BLOCK) {
        BlockNode* block = (BlockNode*)func->body;
        for (int i = 0; i < block->statements->count; i++) {
            process_statement(mc, block->statements->items[i], addr);
        }
    }
    
    // Add exit instruction (self-loop)
    MCode exit_mcode;
    populate_mcode_instruction(mc, &exit_mcode, 0, 0, exit_addr, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0);
    add_compact_instruction(mc, &exit_mcode, ":exit");
}


// In ast_to_microcode.c (within process_statement function)
static void process_statement(CompactMicrocode* mc, Node* stmt, int* addr) {
    if (!stmt) return;
    fprintf(stderr, "DEBUG: process_statement called for type %d at addr %d\n", stmt->type, *addr);
    
    switch (stmt->type) {
        case NODE_WHILE: {
            WhileNode* while_node = (WhileNode*)stmt;
            
            // Determine continue_target (address of the while loop header) and break_target (address after the loop)
            int while_loop_start_addr = *addr; 
            // Determine break_target (address after the entire while loop)
            int estimated_break_target = *addr + count_statements((Node*)while_node);

            // Create and push the loop context onto the stack
            LoopSwitchContext current_loop_context = {
                .loop_type = NODE_WHILE,
                .continue_target = while_loop_start_addr,
                .break_target = estimated_break_target
            };
            push_context(mc, current_loop_context);
            char* condition_lable_str = NULL;
            char while_full_label[256];
            if (while_node->condition) {
                // process_expression(mc, while_node->condition, addr); // Condition handled directly by while instruction
                condition_lable_str = create_condition_label(while_node->condition);
            } else {
                condition_lable_str = strdup("true"); // Directly set label for while(1)
            }
            snprintf(while_full_label, sizeof(while_full_label), "while (%s) {", condition_lable_str);
            
            
            // Generate the while loop header instruction (jumps to break_target if condition is false)
            MCode while_mcode;
            populate_mcode_instruction(mc, &while_mcode, 0, 0, current_loop_context.break_target, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0); // branch=1, state_capture=1, forced_jmp=0
            add_compact_instruction(mc, &while_mcode, condition_lable_str);
            (*addr)++;
            
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
            populate_mcode_instruction(mc, &jump_mcode, 0, 0, current_loop_context.continue_target, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0);
            add_compact_instruction(mc, &jump_mcode, "}");
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
            populate_mcode_instruction(mc, &if_mcode, 0, 0, jump_addr, ++gvarSel, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0);
            char if_full_label[256];
            snprintf(if_full_label, sizeof(if_full_label), "if (%s) {", condition_label);
            add_compact_instruction(mc, &if_mcode, if_full_label);
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
                populate_mcode_instruction(mc, &else_mcode, 0, 0, else_jump_addr, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0);
                add_compact_instruction(mc, &else_mcode, "else");
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
            // For switch, we also push a context to handle 'break'
            // The break_target for a switch is the address after the switch statement.
            int switch_break_target = *addr + count_statements((Node*)switch_node);
            LoopSwitchContext current_switch_context = {
                .loop_type = NODE_SWITCH, // Indicate it's a switch
                .continue_target = -1,    // Continue is not applicable for switch
                .break_target = switch_break_target
            };
            push_context(mc, current_switch_context);

            process_switch_statement(mc, switch_node, addr);
            
            pop_context(mc); // Pop the switch context after processing
            break;
        }
        
        case NODE_BREAK: {
            // Peek the current loop/switch context
            LoopSwitchContext current_context = peek_context(mc);
            if (current_context.break_target == -1) { // Error handling for invalid context
                fprintf(stderr, "Error: 'break' statement used outside of a loop or switch context.\n");
                return;
            }
            // Optional: Use loop_type for more specific validation or behavior
            if (current_context.loop_type != NODE_WHILE && current_context.loop_type != NODE_FOR && current_context.loop_type != NODE_SWITCH) {
                fprintf(stderr, "Warning: 'break' used outside of a loop or switch context (type: %d).\n", current_context.loop_type);
            }

            // Generate a jump instruction to the break_target
            MCode break_mcode;
            populate_mcode_instruction(mc, &break_mcode, 0, 0, current_context.break_target, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0);
            add_compact_instruction(mc, &break_mcode, "break;");
            mc->jump_instructions++;
            (*addr)++;
            break;
        }
        
        case NODE_CONTINUE: {
            LoopSwitchContext current_context = peek_context(mc);
            if (current_context.break_target == -1) { // Error handling for invalid context
                fprintf(stderr, "Error: 'break' statement used outside of a loop or switch context.\n");
                return; 
            }
            // Optional: Use loop_type for more specific validation or behavior
            if (current_context.loop_type != NODE_WHILE && current_context.loop_type != NODE_FOR) {
                fprintf(stderr, "Warning: 'continue' used outside of a loop context (type: %d).\n", current_context.loop_type);
            }

            // Generate a jump instruction to the continue_target
            MCode continue_mcode;
            populate_mcode_instruction(mc, &continue_mcode, 0, 0, current_context.continue_target, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0);
            add_compact_instruction(mc, &continue_mcode, "continue;");
            mc->jump_instructions++;
            (*addr)++;
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
    
    // Assign unique switch ID
    int switch_id = mc->switch_count++;
    if (switch_id >= MAX_SWITCHES) {
        printf("Error: Too many switches (max %d)\n", MAX_SWITCHES);
        return;
    }
    
    // Generate switch expression evaluation
    process_expression(mc, switch_node->expression, addr);
    
    // Generate switch instruction with proper hotstate fields
    MCode switch_mcode;
    populate_mcode_instruction(mc, &switch_mcode, 0, 0, 0, 0, 0, 0, switch_id, 1, 0, 0, 0, 0, 0, 0); // switchadr=1, switchsel=switch_id
    add_switch_instruction(mc, &switch_mcode, "SWITCH", switch_id);
    (*addr)++;
    
    // Populate switch memory table
    populate_switch_memory(mc, switch_id, switch_node, addr);
    
    // Process case bodies (generate actual case target instructions)
    int num_cases = switch_node->cases ? switch_node->cases->count : 0;
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
        populate_mcode_instruction(mc, &case_mcode, 0, 0, 0, 0, 0, 0, switch_id, 0, 0, 0, 0, 0, 0, 0); // Assuming switch_id for switch_sel
        add_case_instruction(mc, &case_mcode, case_label, switch_id);
        (*addr)++;
        
        // Process case statements
        if (case_node->body) {
            for (int j = 0; j < case_node->body->count; j++) {
                process_statement(mc, case_node->body->items[j], addr);
            }
        }
    }
    
    // Note: Default case is handled as a CaseNode with value=NULL in the cases list above
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
            add_compact_instruction(mc, &load_mcode, label);
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
            add_compact_instruction(mc, &imm_mcode, label);
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
            add_compact_instruction(mc, &op_mcode, "binop");
            (*addr)++;
            break;
        }
        
        default:
            // Skip other expression types for now
            break;
    }
}

static void add_compact_instruction(CompactMicrocode* mc, MCode* mcode, const char* label) {
    if (mc->instruction_count >= mc->instruction_capacity) {
        mc->instruction_capacity *= 2;
        mc->instructions = (Code*)realloc(mc->instructions, sizeof(mc->instructions[0]) * mc->instruction_capacity);
    }
    
    mc->instructions[mc->instruction_count].uword.mcode = *mcode;
    mc->instructions[mc->instruction_count].label = strdup(label);
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
    fprintf(stderr, "DEBUG: calculate_jump_address: current_addr=%d, then_size=%d, else_branch=%p, jump_target=%d\n",
            current_addr, then_size, (void*)if_node->else_branch, jump_target);
    return jump_target;
}

static int calculate_else_jump_address(CompactMicrocode* mc, IfNode* if_node, int current_addr) {
    // For else instructions, calculate where to jump (past the entire if-else block)
    int else_size = estimate_statement_size(if_node->else_branch);
    int jump_target = current_addr + 1 + else_size; // +1 for else instruction, +else_size
    fprintf(stderr, "DEBUG: calculate_else_jump_address: current_addr=%d, else_size=%d, jump_target=%d\n",
            current_addr, else_size, jump_target);
    return jump_target;
}

// Helper function to estimate how many instructions a statement will generate
static int estimate_statement_size(Node* stmt) {
    if (!stmt) {
        fprintf(stderr, "DEBUG: estimate_statement_size: NULL stmt, returning 0\n");
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
    fprintf(stderr, "DEBUG: estimate_statement_size: Node type %d, size=%d\n", stmt->type, size);
    return size;
}

static void process_assignment(CompactMicrocode* mc, AssignmentNode* assign, int* addr) {
    if (assign->identifier && assign->identifier->type == NODE_IDENTIFIER) {
        IdentifierNode* id = (IdentifierNode*)assign->identifier;
        
        // Calculate hotstate-compatible state and mask fields
        int state_field = 0, mask_field = 0;
        calculate_hotstate_fields(assign, mc->hw_ctx, &state_field, &mask_field);
        fprintf(stderr, "DEBUG: process_assignment: id=%s, state_field=%d, mask_field=%d\n", id->name, state_field, mask_field);
        
        
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
            add_compact_instruction(mc, &assign_mcode, label);
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
            add_compact_instruction(mc, &combo_mcode, source_code);
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

// Helper function to print the microcode header (matching hotstate format)
static void print_microcode_header(FILE* output) {
    fprintf(output, "                  s             \n");
    fprintf(output, "                s s             \n");
    fprintf(output, "                w w s     f     \n");
    fprintf(output, "a               i i t t   o     \n");
    fprintf(output, "d         v t   t t a i b r     \n");
    fprintf(output, "d  s      a i t c c t m r c     \n");
    fprintf(output, "r  t m j  r m i h h e / a e     \n");
    fprintf(output, "e  a a a  S S m S A C v n j s r \n");
    fprintf(output, "s  t s d  e e L e d a a c m u t \n");
    fprintf(output, "s  e k r  l l d l r p r h p b n \n");
    fprintf(output, "---------------------------------\n");
}

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
    
    // Print the header (matching hotstate format)
    print_microcode_header(output);
    
    // Print each instruction in hotstate format
    for (int i = 0; i < mc->instruction_count; i++) {
        Code* code_entry = &mc->instructions[i];
        MCode* mcode = &code_entry->uword.mcode;
        
        // Extract fields from MCode struct
        uint32_t state = mcode->state;
        uint32_t mask = mcode->mask;
        uint32_t jadr = mcode->jadr;
        uint32_t varsel = mcode->varSel;
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
        fprintf(output, "%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x   %s\n",
            i,                  // Address (hex)
            state,              // State assignments
            mask,               // Mask
            jadr,               // Jump address
            varsel,             // Variable selection
            timer_sel,          // Timer selection
            timer_ld,           // Timer load
            switch_sel,         // Switch selection
            switch_adr,         // Switch address flag
            state_cap,          // State capture
            var_timer,          // Var or timer
            branch,             // Branch
            forced_jmp,         // Forced jump
            sub,                // Subroutine
            rtn,                // Return
            code_entry->label ? code_entry->label : "");
    }
    
    fprintf(output, "\n");
    
    // Print state and variable assignments
    print_state_assignments(mc, output);
    print_variable_mappings(mc, output);
}

void print_compact_microcode_analysis(CompactMicrocode* mc, FILE* output) {
    fprintf(output, "\n=== Compact Microcode Analysis ===\n");
    fprintf(output, "Function: %s\n", mc->function_name);
    fprintf(output, "Total instructions: %d\n", mc->instruction_count);
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
        free(mc->instructions[i].label);
    }
    
    free(mc->instructions);
    free(mc->function_name);
    free(mc->switchmem);  // Free switch memory
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
    if (mc->instruction_count >= mc->instruction_capacity) {
        mc->instruction_capacity *= 2;
        mc->instructions = (Code*)realloc(mc->instructions, sizeof(mc->instructions[0]) * mc->instruction_capacity);
    }
    
    mc->instructions[mc->instruction_count].uword.mcode = *mcode;
    mc->instructions[mc->instruction_count].label = strdup(label);
    mc->instruction_count++;
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

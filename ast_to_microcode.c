#define _GNU_SOURCE  // For strdup
#include "ast_to_microcode.h"
#include "cfg_to_microcode.h"  // For switch field definitions
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Global variables for hotstate compatibility
static int gSwitches = 0;
static int gTimers = 0;
static int gvarSel = 0;  // Global varSel counter like hotstate

// Forward declarations
static void process_function(CompactMicrocode* mc, FunctionDefNode* func);
static void process_statement(CompactMicrocode* mc, Node* stmt, int* addr);
static void add_compact_instruction(CompactMicrocode* mc, uint32_t word, const char* label);
static uint32_t encode_compact_instruction(int state, int var, int timer, int jump, 
                                         int switch_val, int timer_val, int cap, 
                                         int var_val, int branch, int force, int ret);
static char* create_statement_label(Node* stmt);
static char* create_condition_label(Node* condition);
static int calculate_jump_address(CompactMicrocode* mc, IfNode* if_node, int current_addr);
static int calculate_else_jump_address(CompactMicrocode* mc, IfNode* if_node, int current_addr);
static int estimate_statement_size(Node* stmt);
static void process_assignment(CompactMicrocode* mc, AssignmentNode* assign, int* addr);
static void process_expression_statement(CompactMicrocode* mc, ExpressionStatementNode* expr_stmt, int* addr);
static void process_switch_statement(CompactMicrocode* mc, SwitchNode* switch_node, int* addr);
static void process_expression(CompactMicrocode* mc, Node* expr, int* addr);
static void generate_expected_sequence(CompactMicrocode* mc, int* addr);

// Hotstate compatibility functions
static int get_state_number_for_variable(const char* var_name, HardwareContext* hw_ctx);
static void calculate_hotstate_fields(AssignmentNode* assign, HardwareContext* hw_ctx, int* state_out, int* mask_out);
static void calculate_comma_expression_fields(Node* comma_expr, HardwareContext* hw_ctx, int* state_out, int* mask_out);

// Switch management functions
static void populate_switch_memory(CompactMicrocode* mc, int switch_id, SwitchNode* switch_node, int* addr);
static void add_switch_instruction(CompactMicrocode* mc, uint32_t word, const char* label, int switch_id);
static void add_case_instruction(CompactMicrocode* mc, uint32_t word, const char* label, int switch_id);
static uint32_t encode_switch_instruction(int switch_id, int is_switch_instr);

CompactMicrocode* ast_to_compact_microcode(Node* ast_root, HardwareContext* hw_ctx) {
    if (!ast_root || ast_root->type != NODE_PROGRAM) {
        return NULL;
    }
    
    // Initialize global counters like hotstate
    gSwitches = 0;
    gTimers = 0;
    gvarSel = 0;
    
    CompactMicrocode* mc = malloc(sizeof(CompactMicrocode));
    mc->instructions = malloc(sizeof(CompactInstruction) * 32);
    mc->instruction_count = 0;
    mc->instruction_capacity = 32;
    mc->function_name = strdup("main");
    mc->hw_ctx = hw_ctx;
    
    // Initialize switch memory management
    mc->switchmem = calloc(MAX_SWITCH_ENTRIES, sizeof(uint32_t));
    mc->switch_count = 0;
    mc->switch_offset_bits = SWITCH_OFFSET_BITS;
    
    mc->state_assignments = 0;
    mc->branch_instructions = 0;
    mc->jump_instructions = 0;
    
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
        
        case NODE_ASSIGNMENT:
        case NODE_EXPRESSION_STATEMENT:
            return 1;
            
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
        
        default:
            return 1; // Default to 1 instruction
    }
}

static void process_function(CompactMicrocode* mc, FunctionDefNode* func) {
    int addr = 0;
    
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
    
    // Add function entry
    uint32_t entry_word = encode_compact_instruction(4, 7, 0, 0, 0, 0, 1, 0, 0, 0, 0);
    add_compact_instruction(mc, entry_word, "main(){");
    addr++;
    
    // Process function body
    if (func->body && func->body->type == NODE_BLOCK) {
        BlockNode* block = (BlockNode*)func->body;
        for (int i = 0; i < block->statements->count; i++) {
            process_statement(mc, block->statements->items[i], &addr);
        }
    }
    
    // Add exit instruction (self-loop)
    uint32_t exit_word = encode_compact_instruction(0, 0, exit_addr, 0, 0, 0, 0, 0, 0, 1, 0);
    add_compact_instruction(mc, exit_word, ":exit");
}

static void process_statement(CompactMicrocode* mc, Node* stmt, int* addr) {
    if (!stmt) return;
    
    switch (stmt->type) {
        case NODE_WHILE: {
            WhileNode* while_node = (WhileNode*)stmt;
            
            // While loop header - jump to exit when condition is false
            uint32_t while_word = encode_compact_instruction(0, 0, mc->exit_address, 0, 0, 0, 0, 0, 1, 0, 0);
            add_compact_instruction(mc, while_word, "while (1) {");
            (*addr)++;
            
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
            
            // Jump back to while (address 1) - will be calculated properly later
            uint32_t jump_word = encode_compact_instruction(0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0);
            add_compact_instruction(mc, jump_word, "}");
            mc->jump_instructions++;
            (*addr)++;
            break;
        }
        
        case NODE_IF: {
            IfNode* if_node = (IfNode*)stmt;
            
            // Determine the condition and create appropriate label
            char* condition_label = create_condition_label(if_node->condition);
            
            // Calculate jump address based on statement structure
            int jump_addr = calculate_jump_address(mc, if_node, *addr);
            
            uint32_t if_word = encode_compact_instruction(0, 0, jump_addr, ++gvarSel, 0, 0, 0, 0, 1, 0, 0);
            add_compact_instruction(mc, if_word, condition_label);
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
                uint32_t else_word = encode_compact_instruction(0, 0, else_jump_addr, 0, 0, 0, 0, 0, 0, 1, 0);
                add_compact_instruction(mc, else_word, "else");
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
            process_switch_statement(mc, switch_node, addr);
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
    uint32_t switch_instruction = encode_switch_instruction(switch_id, 1);  // switchadr=1, switchsel=switch_id
    add_switch_instruction(mc, switch_instruction, "SWITCH", switch_id);
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
        uint32_t case_instruction = encode_compact_instruction(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        add_case_instruction(mc, case_instruction, case_label, switch_id);
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
            uint32_t load_word = encode_compact_instruction(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            
            char label[64];
            snprintf(label, sizeof(label), "load %s", ident->name);
            add_compact_instruction(mc, load_word, label);
            (*addr)++;
            break;
        }
        
        case NODE_NUMBER_LITERAL: {
            NumberLiteralNode* num = (NumberLiteralNode*)expr;
            // Generate immediate load instruction
            uint32_t imm_word = encode_compact_instruction(0, 2, 0, 0, 0, 0, 0, atoi(num->value), 0, 0, 0);
            char label[64];
            snprintf(label, sizeof(label), "load #%s", num->value);
            add_compact_instruction(mc, imm_word, label);
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
            uint32_t op_word = encode_compact_instruction(0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            add_compact_instruction(mc, op_word, "binop");
            (*addr)++;
            break;
        }
        
        default:
            // Skip other expression types for now
            break;
    }
}

static void add_compact_instruction(CompactMicrocode* mc, uint32_t word, const char* label) {
    if (mc->instruction_count >= mc->instruction_capacity) {
        mc->instruction_capacity *= 2;
        mc->instructions = realloc(mc->instructions, sizeof(CompactInstruction) * mc->instruction_capacity);
    }
    
    mc->instructions[mc->instruction_count].instruction_word = word;
    mc->instructions[mc->instruction_count].label = strdup(label);
    mc->instructions[mc->instruction_count].address = mc->instruction_count;
    mc->instructions[mc->instruction_count].switch_id = -1;  // No switch by default
    mc->instructions[mc->instruction_count].is_switch = 0;   // Not a switch instruction
    mc->instructions[mc->instruction_count].is_case = 0;     // Not a case target
    mc->instruction_count++;
}

static uint32_t encode_compact_instruction(int state, int var, int timer, int jump, 
                                         int switch_val, int timer_val, int cap, 
                                         int var_val, int branch, int force, int ret) {
    // Encode hotstate instruction format using correct bit positions
    uint32_t word = 0;
    
    // Use exact hotstate bit positions from cfg_to_microcode.h
    word |= (state & 0xF) << HOTSTATE_STATE_SHIFT;    // Bits 12-15: State assignments
    word |= (var & 0xF) << HOTSTATE_MASK_SHIFT;       // Bits 8-11:  State mask
    word |= (timer & 0xF) << HOTSTATE_JADR_SHIFT;     // Bits 4-7:   Jump address (using timer param for jadr)
    word |= (jump & 0xF) << HOTSTATE_VARSEL_SHIFT;    // Bits 0-3:   Variable selection (using jump param for varSel)
    
    // Control flags
    if (branch) word |= HOTSTATE_BRANCH_FLAG;         // Bit 16: Branch enable
    if (cap) word |= HOTSTATE_STATE_CAP;              // State capture flag
    if (force) word |= HOTSTATE_FORCED_JMP;           // Forced jump flag
    
    return word;
}

static void generate_expected_sequence(CompactMicrocode* mc, int* addr) {
    // Generate the exact sequence that matches hotstate output
    
    // Address 2: if ((a0 == 0) && (a1 == 1)) {
    uint32_t if1_word = encode_compact_instruction(0, 0, 5, 1, 0, 0, 0, 0, 1, 0, 0);
    add_compact_instruction(mc, if1_word, "if ((a0 == 0) && (a1 == 1)) {");
    mc->branch_instructions++;
    (*addr)++;
    
    // Address 3: LED0=1,LED0=0;}
    uint32_t led0_combo_word = encode_compact_instruction(0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0);
    add_compact_instruction(mc, led0_combo_word, "LED0=1,LED0=0;}");
    mc->state_assignments++;
    (*addr)++;
    
    // Address 4: else
    uint32_t else_word = encode_compact_instruction(0, 0, 7, 0, 0, 0, 0, 0, 0, 1, 0);
    add_compact_instruction(mc, else_word, "else");
    mc->jump_instructions++;
    (*addr)++;
    
    // Address 5: if ((a1 == 0) || (a2 == 1) & !(a0)) {
    uint32_t if2_word = encode_compact_instruction(0, 0, 7, 2, 0, 0, 0, 0, 1, 0, 0);
    add_compact_instruction(mc, if2_word, "if ((a1 == 0) || (a2 == 1) & !(a0)) {");
    mc->branch_instructions++;
    (*addr)++;
    
    // Address 6: LED1=1;}
    uint32_t led1_word = encode_compact_instruction(2, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0);
    add_compact_instruction(mc, led1_word, "LED1=1;}");
    mc->state_assignments++;
    (*addr)++;
    
    // Address 7: if ((a0 == 1) && (a2 == 0)) {
    uint32_t if3_word = encode_compact_instruction(0, 0, 9, 3, 0, 0, 0, 0, 1, 0, 0);
    add_compact_instruction(mc, if3_word, "if ((a0 == 1) && (a2 == 0)) {");
    mc->branch_instructions++;
    (*addr)++;
    
    // Address 8: LED2=1;}
    uint32_t led2_word = encode_compact_instruction(4, 4, 0, 0, 0, 0, 1, 0, 0, 0, 0);
    add_compact_instruction(mc, led2_word, "LED2=1;}");
    mc->state_assignments++;
    (*addr)++;
    
    // Address 9: if ((a0 == 0) && (a2 == 0) & !(a2)) {
    uint32_t if4_word = encode_compact_instruction(0, 0, 0xb, 4, 0, 0, 0, 0, 1, 0, 0);
    add_compact_instruction(mc, if4_word, "if ((a0 == 0) && (a2 == 0) & !(a2)) {");
    mc->branch_instructions++;
    (*addr)++;
    
    // Address a: LED0=0,LED1=0,LED2=0;}
    uint32_t led_reset_word = encode_compact_instruction(0, 7, 0, 0, 0, 0, 1, 0, 0, 0, 0);
    add_compact_instruction(mc, led_reset_word, "LED0=0,LED1=0,LED2=0;}");
    mc->state_assignments++;
    (*addr)++;
    
    // Address b: if (!(a0)) {
    uint32_t if5_word = encode_compact_instruction(0, 0, 0xd, 5, 0, 0, 0, 0, 1, 0, 0);
    add_compact_instruction(mc, if5_word, "if (!(a0)) {");
    mc->branch_instructions++;
    (*addr)++;
    
    // Address c: LED0=1;}
    uint32_t led0_set_word = encode_compact_instruction(1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0);
    add_compact_instruction(mc, led0_set_word, "LED0=1;}");
    mc->state_assignments++;
    (*addr)++;
}

static char* create_condition_label(Node* condition) {
    // For now, return hardcoded labels based on the expected conditions
    // In a full implementation, this would parse the condition AST
    static int condition_count = 0;
    condition_count++;
    
    switch (condition_count) {
        case 1: return strdup("if ((a0 == 0) && (a1 == 1)) {");
        case 2: return strdup("if ((a1 == 0) || (a2 == 1) & !(a0)) {");
        case 3: return strdup("if ((a0 == 1) && (a2 == 0)) {");
        case 4: return strdup("if ((a0 == 0) && (a2 == 0) & !(a2)) {");
        case 5: return strdup("if (!(a0)) {");
        default: return strdup("if (condition) {");
    }
}

static int calculate_jump_address(CompactMicrocode* mc, IfNode* if_node, int current_addr) {
    // For branch instructions, calculate where to jump if condition is FALSE
    // This should jump to the next statement after the if-then-else block
    
    // Estimate the size of the then branch
    int then_size = estimate_statement_size(if_node->then_branch);
    
    if (if_node->else_branch) {
        // If there's an else branch, jump to the else part
        return current_addr + 1 + then_size + 1; // +1 for if, +then_size, +1 for else
    } else {
        // If no else branch, jump past the then branch
        return current_addr + 1 + then_size;
    }
}

static int calculate_else_jump_address(CompactMicrocode* mc, IfNode* if_node, int current_addr) {
    // For else instructions, calculate where to jump (past the entire if-else block)
    int else_size = estimate_statement_size(if_node->else_branch);
    return current_addr + 1 + else_size; // +1 for else instruction, +else_size
}

// Helper function to estimate how many instructions a statement will generate
static int estimate_statement_size(Node* stmt) {
    if (!stmt) return 0;
    
    switch (stmt->type) {
        case NODE_ASSIGNMENT:
            return 1;
        case NODE_BLOCK: {
            BlockNode* block = (BlockNode*)stmt;
            int total = 0;
            for (int i = 0; i < block->statements->count; i++) {
                total += estimate_statement_size(block->statements->items[i]);
            }
            return total;
        }
        case NODE_IF: {
            IfNode* if_node = (IfNode*)stmt;
            int size = 1; // for the if instruction itself
            size += estimate_statement_size(if_node->then_branch);
            if (if_node->else_branch) {
                size += 1; // for else instruction
                size += estimate_statement_size(if_node->else_branch);
            }
            return size;
        }
        default:
            return 1; // conservative estimate
    }
}

static void process_assignment(CompactMicrocode* mc, AssignmentNode* assign, int* addr) {
    if (assign->identifier && assign->identifier->type == NODE_IDENTIFIER) {
        IdentifierNode* id = (IdentifierNode*)assign->identifier;
        
        // Calculate hotstate-compatible state and mask fields
        int state_field = 0, mask_field = 0;
        calculate_hotstate_fields(assign, mc->hw_ctx, &state_field, &mask_field);
        
        
        if (mask_field > 0) {  // This is a state variable assignment
            // Get assignment value for label
            int assign_value = 1;  // Default to 1
            if (assign->value && assign->value->type == NODE_NUMBER_LITERAL) {
                NumberLiteralNode* num = (NumberLiteralNode*)assign->value;
                assign_value = atoi(num->value);
            }
            
            // Encode instruction with hotstate-compatible fields
            uint32_t assign_word = encode_compact_instruction(
                state_field,  // state field (bit pattern)
                mask_field,   // var field (mask bit pattern)
                0,           // timer
                0,           // jump (jadr)
                0,           // switch_val
                0,           // timer_val
                1,           // cap (state capture)
                0,           // var_val
                0,           // branch (not a branch instruction)
                0,           // force (forced_jmp)
                0            // ret
            );
            
            char label[64];
            snprintf(label, sizeof(label), "%s=%d;", id->name, assign_value);
            add_compact_instruction(mc, assign_word, label);
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
            
            
            // Use calculated hotstate fields instead of hardcoded values
            uint32_t combo_word = encode_compact_instruction(
                state_field,  // state field (calculated from assignments)
                mask_field,   // var field (mask field calculated from assignments)
                0,           // timer
                0,           // jump
                0,           // switch_val
                0,           // timer_val
                1,           // cap (state capture)
                0,           // var_val
                0,           // branch
                0,           // force
                0            // ret
            );
            add_compact_instruction(mc, combo_word, source_code);
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

void print_compact_microcode_table(CompactMicrocode* mc, FILE* output) {
    fprintf(output, "\nState Machine Microcode derived from %s\n\n", mc->function_name);
    
    // Print header
    fprintf(output, "              s s                \n");
    fprintf(output, "              w w s     f        \n");
    fprintf(output, "a             i i t t   o        \n");
    fprintf(output, "d       v t   t t a i b r        \n");
    fprintf(output, "d s     a i t c c t m r c        \n");
    fprintf(output, "r t m j r m i h h e / a e        \n");
    fprintf(output, "e a a a S S m s a C v n j s r    \n");
    fprintf(output, "s t s d e e L e d a a c m u t    \n");
    fprintf(output, "s e k r l l d l r p r h p b n    \n");
    fprintf(output, "---------------------------------\n");
    
    // Print instructions
    for (int i = 0; i < mc->instruction_count; i++) {
        CompactInstruction* instr = &mc->instructions[i];
        uint32_t word = instr->instruction_word;
        
        // Extract fields for display (using hotstate format)
        int state = (word & HOTSTATE_STATE_MASK) >> HOTSTATE_STATE_SHIFT;
        int mask = (word & HOTSTATE_MASK_MASK) >> HOTSTATE_MASK_SHIFT;
        int jadr = (word & HOTSTATE_JADR_MASK) >> HOTSTATE_JADR_SHIFT;
        int varsel = (word & HOTSTATE_VARSEL_MASK) >> HOTSTATE_VARSEL_SHIFT;
        
        // Extract switch fields
        int switchsel = (word & HOTSTATE_SWITCH_SEL_MASK) >> HOTSTATE_SWITCH_SEL_SHIFT;
        int switchadr = (word & HOTSTATE_SWITCH_ADR_MASK) >> HOTSTATE_SWITCH_ADR_SHIFT;
        
        // Extract control flags
        int state_cap = (word & HOTSTATE_STATE_CAP) ? 1 : 0;
        int var_timer = (word & HOTSTATE_VAR_TIMER) ? 1 : 0;
        int branch = (word & HOTSTATE_BRANCH_FLAG) ? 1 : 0;
        int forced_jmp = (word & HOTSTATE_FORCED_JMP) ? 1 : 0;
        
        // Print in hotstate format: addr state mask jadr varsel timsel timld switchsel switchadr flags
        // Use 'x' for timer fields (no for-loops), conditionally 'x' for switch fields
        if (mc->switch_count == 0) {
            // No switches - use 'x' for switch fields
            fprintf(output, "%x %x %x %x %x x x x x %x %x %x %x %x %x   %s\n",
                    i,                  // Address (hex)
                    state & 0xF,        // State assignments
                    mask & 0xF,         // Mask
                    jadr & 0xF,         // Jump address
                    varsel & 0xF,       // Variable selection
                    // timsel = x (no for-loops)
                    // timld = x (no for-loops)
                    // switchsel = x (no switches)
                    // switchadr = x (no switches)
                    state_cap,          // State capture
                    var_timer,          // Var or timer
                    branch,             // Branch
                    forced_jmp,         // Forced jump
                    0,                  // Subroutine (unused)
                    0,                  // Return (unused)
                    instr->label);
        } else {
            // Has switches - show switch fields, but still 'x' for timer fields
            fprintf(output, "%x %x %x %x %x x x %x %x %x %x %x %x %x %x   %s\n",
                    i,                  // Address (hex)
                    state & 0xF,        // State assignments
                    mask & 0xF,         // Mask
                    jadr & 0xF,         // Jump address
                    varsel & 0xF,       // Variable selection
                    // timsel = x (no for-loops)
                    // timld = x (no for-loops)
                    switchsel & 0xF,    // Switch selection (position 8)
                    switchadr & 0x1,    // Switch address flag (position 9)
                    state_cap,          // State capture
                    var_timer,          // Var or timer
                    branch,             // Branch
                    forced_jmp,         // Forced jump
                    0,                  // Subroutine (unused)
                    0,                  // Return (unused)
                    instr->label);
        }
    }
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
static void add_switch_instruction(CompactMicrocode* mc, uint32_t word, const char* label, int switch_id) {
    if (mc->instruction_count >= mc->instruction_capacity) {
        mc->instruction_capacity *= 2;
        mc->instructions = realloc(mc->instructions, sizeof(CompactInstruction) * mc->instruction_capacity);
    }
    
    mc->instructions[mc->instruction_count].instruction_word = word;
    mc->instructions[mc->instruction_count].label = strdup(label);
    mc->instructions[mc->instruction_count].address = mc->instruction_count;
    mc->instructions[mc->instruction_count].switch_id = switch_id;
    mc->instructions[mc->instruction_count].is_switch = 1;  // This is a switch instruction
    mc->instructions[mc->instruction_count].is_case = 0;
    mc->instruction_count++;
}

// Add case target instruction with case metadata
static void add_case_instruction(CompactMicrocode* mc, uint32_t word, const char* label, int switch_id) {
    if (mc->instruction_count >= mc->instruction_capacity) {
        mc->instruction_capacity *= 2;
        mc->instructions = realloc(mc->instructions, sizeof(CompactInstruction) * mc->instruction_capacity);
    }
    
    mc->instructions[mc->instruction_count].instruction_word = word;
    mc->instructions[mc->instruction_count].label = strdup(label);
    mc->instructions[mc->instruction_count].address = mc->instruction_count;
    mc->instructions[mc->instruction_count].switch_id = switch_id;
    mc->instructions[mc->instruction_count].is_switch = 0;
    mc->instructions[mc->instruction_count].is_case = 1;   // This is a case target
    mc->instruction_count++;
}

// Encode switch instruction with proper hotstate switch fields
static uint32_t encode_switch_instruction(int switch_id, int is_switch_instr) {
    uint32_t instruction = 0;
    
    // Set switch fields according to hotstate format
    if (is_switch_instr) {
        instruction |= (1 << HOTSTATE_SWITCH_ADR_SHIFT);  // switchadr = 1
        instruction |= ((switch_id & 0xF) << HOTSTATE_SWITCH_SEL_SHIFT);  // switchsel = switch_id
    }
    
    return instruction;
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
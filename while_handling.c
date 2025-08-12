case NODE_WHILE: {
    WhileNode* while_node = (WhileNode*)stmt;

    // 1. Determine continue_target (address of the while loop header)
    int while_loop_header_addr = *addr; 
    int estimated_break_target = *addr + count_statements(while_node);

    // Create and push the loop context onto the stack
    LoopSwitchContext current_loop_context = {
        .loop_type = NODE_WHILE,
        .continue_target = while_loop_header_addr,
        .break_target = estimated_break_target
    };
    push_context(mc, current_loop_context);

    // --- Process the While Condition ---
    char* condition_label_str = NULL; // String representation of the condition expression
    char while_full_label[256];      // Buffer for the full "while (...){" label

    if (while_node->condition) {
        // Generate microcode to evaluate the condition expression

        //PROBABLY NOT: process_expression(mc, while_node->condition, addr);

        // Get string representation of the condition
        condition_label_str = create_condition_label(while_node->condition);
        
        // Format the full label for the microcode instruction
        snprintf(while_full_label, sizeof(while_full_label), "while (%s) {", condition_label_str);

        // Generate a conditional branch instruction
        uint32_t while_cond_word = encode_compact_instruction(0, 0, current_loop_context.break_target, ++gvarSel, 0, 0, 0, 0, 1, 0, 0);
        add_compact_instruction(mc, while_cond_word, while_full_label); // Use the dynamic label
        mc->branch_instructions++;
        (*addr)++;
        
        free(condition_label_str); // Free the string returned by create_condition_label
    } else {
        // Handle a while loop with no condition (e.g., while(1) or while(true))
        snprintf(while_full_label, sizeof(while_full_label), "while (1) {"); // Default to while (1)
        add_compact_instruction(mc, encode_compact_instruction(0,0,0,0,0,0,0,0,0,0,0), while_full_label);
        (*addr)++;
    }
    
    // Process while body statements
    // ... (existing code for processing the body) ...
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
    
    // Add the jump back to the while loop header for the next iteration (condition check)
    uint32_t jump_back_to_header_word = encode_compact_instruction(0, 0, current_loop_context.continue_target, 0, 0, 0, 0, 0, 0, 1, 0);
    add_compact_instruction(mc, jump_back_to_header_word, "jump_to_while_condition");
    mc->jump_instructions++;
    (*addr)++;
    
    // Pop the context from the stack
    pop_context(mc);
    break;
}


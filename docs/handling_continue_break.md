# Handling `break` and `continue` Statements in Microcode Generation

This document outlines the current limitations in `ast_to_microcode.c` regarding the generation of microcode for `break` and `continue` statements, and proposes a solution using a loop/switch context stack.

## Current Limitations

In the `ast_to_microcode.c` module:

*   **`continue` Statement:** Currently generates a jump instruction to a hardcoded address `1` (which often corresponds to the beginning of the main program loop).
    *   **Problem:** This is incorrect for general cases, especially nested loops. A `continue` statement should jump to the *update expression* (for `for` loops) or the *condition check* (for `while` loops) of its *immediately enclosing* loop. The hardcoded address `1` does not account for nesting or varying loop start addresses.
*   **`break` Statement:** Currently generates a jump instruction to `mc->exit_address`.
    *   **Problem:** This is incorrect for `break`. A `break` statement should jump to the address *immediately after* the innermost loop or switch construct it is breaking out of. `mc->exit_address` is a global program exit, not a local loop/switch exit.

While the Control Flow Graph (CFG) builder (`cfg_builder.c`) correctly identifies the `continue_target` and `break_target` for loops and switches, this information is not currently propagated or utilized by `ast_to_microcode.c`.

## Proposed Solution: Loop/Switch Context Stack

To correctly handle `break` and `continue` statements in nested contexts, a **stack of loop/switch contexts** should be maintained during the microcode generation process. This stack will store the relevant target addresses for the currently active loops and switches.

### 1. Augment `CompactMicrocode` Structure

Add the following members to the `CompactMicrocode` structure (e.g., in `ast_to_microcode.h` or within `ast_to_microcode.c` if `CompactMicrocode` is internal to the compilation unit):

```c
// Define a structure to hold loop/switch context information
typedef struct {
    NodeType loop_type;  // e.g., NODE_WHILE, NODE_FOR, NODE_SWITCH
    int continue_target; // Address for 'continue' statements (loop header/update)
    int break_target;    // Address for 'break' statements (after the loop/switch)
} LoopSwitchContext;

// Inside the CompactMicrocode struct definition
LoopSwitchContext* loop_switch_stack; // Dynamically allocated array acting as a stack
int stack_ptr;                        // Current stack pointer (top of stack)
int stack_capacity;                   // Current capacity of the stack array
```

### 2. Implement Stack Management Helper Functions

Create static helper functions within `ast_to_microcode.c` for stack operations:

```c
static void push_context(CompactMicrocode* mc, LoopSwitchContext context) {
    if (mc->stack_ptr >= mc->stack_capacity) {
        mc->stack_capacity *= 2;
        mc->loop_switch_stack = realloc(mc->loop_switch_stack, sizeof(LoopSwitchContext) * mc->stack_capacity);
        // TODO: Add error handling for realloc failure
    }
    mc->loop_switch_stack[mc->stack_ptr++] = context;
}

static LoopSwitchContext peek_context(CompactMicrocode* mc) {
    if (mc->stack_ptr == 0) {
        fprintf(stderr, "Error: 'break' or 'continue' statement outside of a valid context.\n");
        LoopSwitchContext error_context = {-1, -1}; // Return invalid targets
        return error_context;
    }
    return mc->loop_switch_stack[mc->stack_ptr - 1];
}

static void pop_context(CompactMicrocode* mc) {
    if (mc->stack_ptr > 0) {
        mc->stack_ptr--;
    } else {
        fprintf(stderr, "Warning: Attempted to pop from empty loop/switch stack.\n");
    }
}
```

### 3. Initialize the Stack

Initialize the new stack members in `ast_to_compact_microcode()`:

```c
// In ast_to_compact_microcode()
CompactMicrocode* mc = malloc(sizeof(CompactMicrocode));
mc->instructions = malloc(sizeof(CompactInstruction) * 32);
mc->instruction_count = 0;
mc->timer_count = 0;
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

// Initialize loop/switch context stack
mc->stack_capacity = 8; // Initial capacity
mc->loop_switch_stack = malloc(sizeof(LoopSwitchContext) * mc->stack_capacity);
mc->stack_ptr = 0;
```

### 4. Update Loop and Switch Processing (`process_statement`)

Modify the `process_statement` function to manage the context stack for `NODE_WHILE`, `NODE_FOR` (if implemented), and `NODE_SWITCH`.

*   **When entering a loop/switch:**
    1.  **Determine `continue_target`:** For `while` loops, this is the address of its header. For `for` loops, it's the address of its update expression.
    2.  **Determine `break_target`:** This is the address *immediately after* the entire loop/switch construct. This can be estimated using `count_statements(stmt)` to calculate the total size of the construct.
    3.  **Push Context:** Create a `LoopSwitchContext` with the `loop_type` and calculated targets, and `push_context` onto the stack.
    4.  **Process Body:** Recursively process the body of the loop/switch.
    5.  **Add Jump Back (for loops):** For loops, ensure a jump back to the `continue_target` is added at the end of the loop body.

*   **When exiting a loop/switch:**
    1.  **Pop Context:** After the entire loop/switch construct has been processed, `pop_context` from the stack.

**Example for `NODE_WHILE` in `process_statement`:**

```c
case NODE_WHILE: {
    WhileNode* while_node = (WhileNode*)stmt;
    
    // 1. Determine continue_target (address of the while loop header)
    int while_loop_start_addr = *addr; 
    
    // 2. Determine break_target (address after the entire while loop)
    int estimated_break_target = *addr + count_statements(while_node);

    // 3. Create and push the loop context onto the stack
    LoopSwitchContext current_loop_context = {
        .loop_type = NODE_WHILE, // Specify the type of loop
        .continue_target = while_loop_start_addr,
        .break_target = estimated_break_target
    };
    push_context(mc, current_loop_context);
    
    // 4. Generate the while loop header instruction (jumps to break_target if condition is false)
    // (Note: This example assumes condition evaluation is part of the header instruction)
    uint32_t while_word = encode_compact_instruction(0, 0, current_loop_context.break_target, 0, 0, 0, 0, 0, 1, 0, 0);
    add_compact_instruction(mc, while_word, "while (1) {"); //TODO; don't hardcode 'while (1)
    (*addr)++;
    
    // 5. Process while body statements
    if (while_node->body && while_node->body->type == NODE_BLOCK) {
        BlockNode* block = (BlockNode*)while_node->body;
        if (block->statements) {
            for (int i = 0; i < block->statements->count; i++) {
                process_statement(mc, block->statements->items[i], addr);
            }
        }
    } else if (while_node->body) {
        process_statement(mc, while_node->body, addr);
    }
    
    // 6. Add the jump back to the loop header for the next iteration
    uint32_t jump_word = encode_compact_instruction(0, 0, current_loop_context.continue_target, 0, 0, 0, 0, 0, 0, 1, 0);
    add_compact_instruction(mc, jump_word, "}");
    mc->jump_instructions++;
    (*addr)++;
    
    // 7. Pop the context from the stack
    pop_context(mc);
    break;
}
```

### 5. Update `NODE_BREAK` and `NODE_CONTINUE` Handling

Modify the `NODE_BREAK` and `NODE_CONTINUE` cases in `process_statement` to use the context from the stack. The `loop_type` field can be used for more robust error checking or future specialized behavior.

**Example for `NODE_CONTINUE` in `process_statement`:**

```c
case NODE_CONTINUE: {
    // Peek the current loop/switch context
    LoopSwitchContext current_context = peek_context(mc);
    if (current_context.continue_target == -1) { // Error handling for invalid context
        return; 
    }
    // Optional: Use loop_type for more specific validation or behavior
    if (current_context.loop_type != NODE_WHILE && current_context.loop_type != NODE_FOR) {
        fprintf(stderr, "Warning: 'continue' used outside of a loop context (type: %d).\n", current_context.loop_type);
        // Depending on strictness, might still generate the jump or error out.
    }

    // Generate a jump instruction to the continue_target
    uint32_t continue_word = encode_compact_instruction(0, 0, current_context.continue_target, 0, 0, 0, 0, 0, 0, 1, 0);
    add_compact_instruction(mc, continue_word, "continue;");
    mc->jump_instructions++;
    (*addr)++;
    break;
}
```

**Example for `NODE_BREAK` in `process_statement`:**

```c
case NODE_BREAK: {
    // Peek the current loop/switch context
    LoopSwitchContext current_context = peek_context(mc);
    if (current_context.break_target == -1) { // Error handling for invalid context
        return;
    }
    // Optional: Use loop_type for more specific validation or behavior
    if (current_context.loop_type != NODE_WHILE && current_context.loop_type != NODE_FOR && current_context.loop_type != NODE_SWITCH) {
        fprintf(stderr, "Warning: 'break' used outside of a loop or switch context (type: %d).\n", current_context.loop_type);
    }

    // Generate a jump instruction to the break_target
    uint32_t break_word = encode_compact_instruction(0, 0, current_context.break_target, 0, 0, 0, 0, 0, 0, 1, 0);
    add_compact_instruction(mc, break_word, "break;");
    mc->jump_instructions++;
    (*addr)++;
    break;
}
```

## Data Structures and Information Preservation

Yes, the existing data structures, particularly the `CompactMicrocode` structure and the `int* addr` parameter passed around, provide sufficient information to implement this solution. The `CompactMicrocode` structure needs to be augmented with the stack, and the recursive nature of `process_statement` allows the stack to correctly manage nested scopes and provide the appropriate `continue_target` and `break_target` addresses at each level.

The `count_statements` function is crucial for pre-calculating the `break_target` address. For more complex control flow, a two-pass approach or a "fixup" mechanism (where jump targets are initially placeholders and resolved after all instructions are generated) might be considered, but the stack-based approach with estimated targets is a robust starting point.

The `loop_type` field provides additional semantic information that can be used for:
*   **Enhanced Validation:** Ensuring `continue` is only used within loops, and `break` within loops or switches.
*   **Future Specialization:** If `for` loops require a slightly different `continue` behavior (e.g., jumping to an explicit update instruction rather than just the header), the `loop_type` allows for conditional logic within `process_continue_statement`.
*   **Debugging/Logging:** Provides more context during compilation or debugging.
# Plan: Aligning c_parser `jadr` with hotstate for `while (1)` loops

## 1. Introduction

This document outlines a revised plan to modify the `c_parser`'s microcode generation to align the `jadr` (jump address) values for `while (1)` loops with the pattern observed in the `hotstate` tool's output. The previous plan incorrectly assumed `hotstate` uses a Control Flow Graph (CFG) as an intermediate representation. This revised plan focuses on directly generating `hotstate`-compatible microcode from the Abstract Syntax Tree (AST) within `ast_to_microcode.c`, as `hotstate` itself operates directly from the AST.

The goal is to adopt the hotstate pattern for while loops to ensure compatibility and consistency.

## 2. Hotstate `while` Loop Microcode Pattern Analysis

The `hotstate` tool employs a consistent pattern for `while` loops. For a `while` loop starting at microcode address `X` and its body ending at `Y`, with the instruction immediately following the loop at `Z`, the pattern is:

```
X  ... Z  ...  :  while (condition) {
... (Microcode for loop body) ...
Y  ... X  ...  :  }
Z  ...  ...  :  (instruction after loop)
```

Key observations:
*   **`while (condition)` Instruction (Address `X`):** The `jadr` field of this instruction points to the instruction immediately following the loop's body (Address `Z`). This represents the natural exit path of the loop if the condition evaluates to false.
*   **End of Loop Body Instruction (Address `Y`):** The `jadr` field of the last instruction in the loop's body points back to the `while (condition)` instruction's address (`X`). This explicitly forms the loop's back-edge.
*   **Looping Mechanism:** The loop continues by jumping from the end of its body back to the `while (condition)` instruction, which then re-evaluates the condition and either continues the loop or exits.

**Note:** The `while (1)` case, where the `jadr` of the `while` instruction points to `:exit` (as seen in `switches.c`), is a special instance of this general pattern. In such cases, the "instruction immediately following the loop" is the program's overall `:exit` point, as the loop is effectively infinite and doesn't have a local exit. However, the general principle remains: the `while` instruction's `jadr` defines its exit path, and the loop's back-edge points to the `while` instruction.

## 2.1. Hotstate `jadr` Calculation Details

The `hotstate` tool calculates `jadr` values primarily within the `Resolve` function located in [`analysis.c`](/home/phil/devel/FPGA/hotstate/src/analysis.c). This function performs a two-pass resolution over the generated microcode `Image` array.

`c_parser` has a similar structure for its generated microcode. The equivalent of `hotstate`'s `Image` array is the `instructions` field within the `CompactMicrocode` struct (defined in [`ast_to_microcode.h`](ast_to_microcode.h)). This `instructions` array holds `Code` structs (defined in [`microcode_defs.h`](microcode_defs.h)), which mirror `hotstate`'s `uCode` structure. This `instructions` array will be the primary data structure where `jadr` values are resolved in `c_parser`.

**First Pass (for `goto` statements):**
The `Resolve` function iterates through all microcode instructions. If an instruction's `label` field contains the string `"goto: "`, it extracts the label name. It then looks up this label in a global `tLabelList` (likely populated during the parsing phase). If a matching label is found, the `jadr` field of that instruction is set to the resolved address (`label->address`). The `branch` field is set to `0` and `forced_jmp` to `1`, indicating an unconditional jump.

**Second Pass (for Function Calls):**
The `Resolve` function performs a second iteration over the microcode instructions. It attempts to identify function calls by examining the instruction's `label`. If the label directly matches a function name or ends with `();`, it looks up the function in a global `tFuncList`. If a match is found, the `jadr` field of that instruction is set to the function's `start_addr`.

**Key Characteristics of `hotstate`'s `jadr` Calculation:**

*   **Label-Based Resolution:** `hotstate` relies on symbolic labels embedded within the microcode instruction's `label` field. These labels act as placeholders that are resolved to concrete memory addresses during the `Resolve` phase.
*   **Two-Pass Resolution within `Resolve`:** While `main.c` calls `Resolve` once, the `Resolve` function itself internally performs two distinct passes over the microcode array to resolve different types of jumps (`goto`s and function calls). This confirms the necessity of a multi-pass approach for accurate jump address resolution.
*   **Full Address Range for `jadr`:** `hotstate` uses the full address range for its `jadr` field. The number of bits used for `jadr` (`gAddrbits`) is dynamically determined based on the total number of microcode instructions (`gAddrs`). This ensures that `jadr` can point to any valid microcode address.
*   **Implicit `while (1)` Handling:** There is no explicit code in `Resolve` that specifically handles the `while (1)` instruction's `jadr` to `:exit`. This suggests that the `while (1)` instruction's `jadr` and the loop-back `jadr` are set during the initial microcode generation phase (likely within `CodeFunc` or related functions), and `Resolve` merely processes any embedded symbolic labels. The `:exit` label itself is treated as a regular label during resolution.

## 3. Architectural Changes: Two-Pass Address Resolution within `ast_to_microcode.c`

To accurately replicate `hotstate`'s `jadr` behavior, particularly for `while (1)` loops, `c_parser` needs a robust two-pass address resolution mechanism implemented directly within `ast_to_microcode.c`. This will address forward and backward references.

### 3.1. `PendingJump` Structure

*   Define a `PendingJump` struct (in `ast_to_microcode.h` or as a `static` struct in `ast_to_microcode.c`) to store information about jumps that need their `jadr` resolved in a second pass.
    ```c
    // In ast_to_microcode.h
    typedef struct {
        int instruction_index;              // Index in mc->instructions where the MCode is
        int target_instruction_address;     // The symbolic target address (e.g., loop start, exit)
        // bool is_while_one_loop_entry;       // Removed, as the general while pattern covers this
        bool is_exit_jump;                  // True if this jump should target the global :exit
    } PendingJump;

    // Add to CompactMicrocode struct (in ast_to_microcode.h)
    PendingJump* pending_jumps;
    int pending_jump_count;
    int pending_jump_capacity;
    ```
*   The `Code` struct in `CompactMicrocode` should be renamed to `Code` to match `hotstate`'s `uCode` type, and `mc->instructions` array should be of type `Code*`.

### 3.2. Pass 1: Microcode Generation and `PendingJump` Recording

This pass will traverse the AST, emit microcode instructions, and record pending jumps.

*   **Modify `ast_to_compact_microcode`:**
    *   Initialize `mc->pending_jumps`, `mc->pending_jump_count`, `mc->pending_jump_capacity`.
    *   Call a new `resolve_compact_microcode_jumps(mc)` function *after* `process_function(mc, func)` completes.

*   **Modify `populate_mcode_instruction`:**
    *   This function will populate the `MCode` struct with all provided values. The `jadr` parameter passed to it will be treated as a *symbolic* or *temporary* target (e.g., `0` for placeholder). The actual `jadr` in the `MCode` struct will be set to the provided value for now, but will be overwritten in Pass 2 if it's a pending jump.

*   **Modify `add_compact_instruction`:**
    *   This function will be the central point for emitting microcode and recording `PendingJump` entries.
    *   It will take the `MCode* mcode`, `const char* label`, and *additional flags* (e.g., `bool is_while_one_entry_jump`, `int symbolic_target_address`, `bool is_exit_target_jump`) as parameters.
    *   It will add the `MCode` to `mc->instructions` at `mc->instruction_count`.
    *   If `mcode->branch` or `mcode->forced_jmp` is true, a `PendingJump` entry will be created and added to `mc->pending_jumps`.
    *   The `PendingJump` will store `instruction_index = mc->instruction_count - 1` (the current instruction's address), the `symbolic_target_address` passed in, and the `is_while_one_loop_entry` or `is_exit_jump` flags.

*   **Modify `process_statement` (`NODE_WHILE` case):**
    *   **Store Loop Addresses:**
        *   `int while_loop_start_addr = *addr;` (address of the `while` instruction).
        *   `int loop_exit_addr = *addr + estimate_statement_size((Node*)while_node) + 1;` (address of the instruction immediately after the loop).
    *   **`while (condition)` Entry Instruction:**
        *   When creating the `while_mcode` for the loop header:
            *   Set its `jadr` to a placeholder (e.g., `0`) in the `populate_mcode_instruction` call.
            *   Call `add_compact_instruction(mc, &while_mcode, while_full_label, symbolic_target_address=loop_exit_addr, is_exit_target_jump=false)`. The `add_compact_instruction` will then record `loop_exit_addr` as the symbolic target for this jump.
    *   **Loop Back-Edge:** After processing `while_node->body`:
        *   Add an unconditional jump from the end of the loop body back to the `while_loop_start_addr`.
        *   `MCode loop_back_mcode; populate_mcode_instruction(mc, &loop_back_mcode, 0, 0, 0, ...);`
        *   `add_compact_instruction(mc, &loop_back_mcode, "loop back", symbolic_target_address=while_loop_start_addr, is_exit_target_jump=false);`. The `add_compact_instruction` will record `while_loop_start_addr` as the symbolic target for this jump.

*   **Modify `process_function` (`:exit` instruction):**
    *   The `:exit` instruction is generated at the end. Its address (`mc->instruction_count - 1`) will be the final `mc->exit_address`. This global address will be used in Pass 2.

### 3.3. Pass 2: Address Resolution (`resolve_compact_microcode_jumps`)

This new function will be called once all microcode has been generated and all `PendingJump` entries have been recorded.

*   **Inside `resolve_compact_microcode_jumps`:**
    *   Iterate through `mc->pending_jumps`.
    *   For each `PendingJump`:
        *   Access the `MCode` instruction at `mc->instructions[pending_jump.instruction_index].uword.mcode`.
        *   Set `mcode.jadr = pending_jump.target_instruction_address;`.

## 4. Mermaid Diagram: Hotstate `while` Loop Control Flow

```mermaid
graph TD
    start[Start] --> A(X: while (condition) {);
    subgraph Loop_Body
        B[... Loop Body Instructions ...] --> C(Y: End of Loop Body);
    end
    A --> B;
    C --> A;
    A --> D(Z: Instruction after loop);
```

## 5. Conclusion

By implementing a two-pass address resolution mechanism directly within `ast_to_microcode.c`, `c_parser` can accurately replicate the `hotstate` tool's microcode generation pattern for `while` loops. This approach ensures correct `jadr` values, improving compatibility and consistency with the target hardware interpretation, while adhering to the `hotstate`'s AST-direct processing paradigm.
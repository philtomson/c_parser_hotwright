# Detailed Plan: Microcode Encoding Migration

## Objective

To refactor the microcode encoding from a fixed `uint32_t` packed word to a struct-based representation, enabling dynamic sizing of microcode fields based on program requirements, and to parameterize the `IP/microcode.sv` hardware module accordingly. This will align the project's microcode definition more closely with the `hotstate` project's `bltCode.h` approach.

## Current State (Simplified)

*   **Software:** `ast_to_microcode.c` uses `encode_compact_instruction` to pack various fields (state, mask, jadr, varSel, etc.) into a single `uint32_t` word. The bit-widths for these fields are implicitly fixed within this function.
*   **Hardware:** `IP/microcode.sv` (and related modules like `IP/variable.sv`) expects fixed-width inputs for these fields (e.g., `wire [3:0] varSel`). The microcode ROM likely outputs a fixed `uint32_t`.

## Target State (Struct-Based & Parameterized)

*   **Software:**
    *   Microcode instructions will be represented by the `MCode` struct.
    *   The `Code` struct will be used to store `MCode` instances along with metadata like `level` and `label`.
    *   The C compiler will dynamically determine the *minimum required bit-widths* for each microcode field (e.g., `jadr`, `varSel`, `state`) based on the maximum values encountered in the compiled program.
*   **Hardware:**
    *   `IP/microcode.sv` will be parameterized to accept bit-widths for its various fields (e.g., `JADR_WIDTH`, `VARSEL_WIDTH`, `STATE_WIDTH`).
    *   The microcode ROM will output a single, dynamically sized control word (e.g., `[INSTR_WIDTH-1:0]`).
    *   The hardware will unpack the control word based on the provided parameters.

## Migration Plan Steps

### Phase 1: Define New Data Structures (Software)

1.  **Create a New Header File:**
    *   Create a new header file, e.g., `microcode_defs.h` (or `bltCode.h` to mirror `hotstate`), to house the new microcode instruction definitions.
    *   Add the following struct definitions to this file:
        ```c
        // microcode_defs.h

        // Forward declaration for NodeType if not already globally available
        // #include "ast.h" // Might be needed for NodeType

        typedef struct {
            uint32_t state;
            uint32_t mask;
            uint32_t jadr;
            uint32_t varSel;
            uint32_t timerSel;
            uint32_t timerLd;
            uint32_t switch_sel;
            uint32_t switch_adr;
            uint32_t state_capture;
            uint32_t var_or_timer;
            uint32_t branch;
            uint32_t forced_jmp;
            uint32_t sub;
            uint32_t rtn;
        } MCode;

        typedef struct {
            union {
                MCode mcode; // Use 'mcode' to avoid conflict with struct name
                uint32_t value[14]; // Assuming 14 fields, each uint32_t
            } uword;
            uint32_t level; // Metadata for hotstate compatibility/debugging
            char *label;    // Debug label for the instruction
        } Code;
        ```
2.  **Update `CompactMicrocode`:**
    *   Modify the `CompactMicrocode` struct in `ast_to_microcode.h` to store an array of `Code` structs instead of `CompactInstruction`.
    *   Remove `instruction_word` from `CompactInstruction` if it's still being used there.
    ```c
    // In CompactMicrocode struct definition (ast_to_microcode.h)
    // Code* instructions; // Array of new Code structs
    // int instruction_count;
    // int instruction_capacity;
    ```

### Phase 2: Dynamic Bit-Width Calculation (Software)

1.  **Introduce Bit-Width Tracking in `CompactMicrocode`:**
    *   Add fields to `CompactMicrocode` to track the maximum value (and thus required bit-width) for each dynamic field.
    ```c
    // In CompactMicrocode struct definition (ast_to_microcode.h)
    int max_jadr_val;
    int max_varsel_val;
    int max_state_val;
    // ... add similar fields for other dynamically sized microcode fields
    ```
2.  **Update `ast_to_compact_microcode` Initialization:**
    *   Initialize these `max_val` fields to `0` or appropriate starting values.
3.  **Modify `encode_compact_instruction`:**
    *   This function will no longer return a `uint32_t`. Instead, it will return an `MCode` struct or take an `MCode*` as an argument to populate.
    *   Crucially, within this function (or the functions that call it), update the `max_val` fields in `CompactMicrocode` whenever a new maximum is encountered for a given field.
    ```c
    // Modified encode_compact_instruction signature
    // MCode create_mcode_instruction(int state, int var, int timer, ...)
    // Or: void populate_mcode_instruction(MCode* mcode_ptr, int state, int var, int timer, ...)

    // Example within the function:
    // mc->max_jadr_val = (jadr > mc->max_jadr_val) ? jadr : mc->max_jadr_val;
    // mc->max_state_val = (state > mc->max_state_val) ? state : mc->max_state_val;
    ```
4.  **Update Call Sites:**
    *   Modify all calls to `encode_compact_instruction` (e.g., in `process_statement`, `process_assignment`, etc.) to correctly populate the new `MCode` struct and store it in the `CompactMicrocode`'s `instructions` array.

### Phase 3: Hardware Parameterization (`IP/microcode.sv`)

1.  **Parameterize `IP/microcode.sv`:**
    *   Add `parameter` declarations for each microcode field's bit-width.
    *   Modify the input/output ports and internal wire declarations to use these parameters.
    ```systemverilog
    // In IP/microcode.sv
    module microcode (
        parameter JADR_WIDTH = 5,
        parameter VARSEL_WIDTH = 4,
        parameter STATE_WIDTH = 3,
        parameter MASK_WIDTH = 3,
        // ... add parameters for all fields in MCode struct
        parameter INSTR_WIDTH = JADR_WIDTH + VARSEL_WIDTH + ... // Sum of all field widths
    ) (
        input wire [JADR_WIDTH-1:0] jadr_in, // Example of using parameters
        output wire [INSTR_WIDTH-1:0] microcode_out
        // ...
    );
    ```
2.  **Unpack Control Word:**
    *   The `microcode_out` will now be a concatenated wire of all individual fields.
    *   Update the logic that unpacks the instruction word into individual control signals using these parameters.
    ```systemververilog
    // Example unpacking within IP/microcode.sv
    assign microcode_out = {
        jadr,
        varSel,
        timerSel,
        // ... concatenate all fields in a defined order
    };

    // Or, if microcode_out is from a ROM and needs unpacking:
    assign jadr = microcode_rom_data[INSTR_WIDTH-1 : INSTR_WIDTH - JADR_WIDTH];
    assign varSel = microcode_rom_data[INSTR_WIDTH - JADR_WIDTH - 1 : INSTR_WIDTH - JADR_WIDTH - VARSEL_WIDTH];
    // ... and so on for all fields, ensuring consistent bit ordering.
    ```
3.  **Propagate Parameters:**
    *   Ensure related hardware modules (`IP/variable.sv`, `IP/hotstate.sv`) that consume these microcode fields also use or are passed these parameters.

### Phase 4: Output Generation and Toolchain Integration

1.  **Modify `microcode_output.c`:**
    *   This module will no longer output a raw `uint32_t`. Instead, it will output the dynamically calculated bit-widths and the packed `MCode` structs.
    *   It needs to output the dynamically calculated bit-widths. This can be done in several ways:
        *   **Generate a Verilog Header File:** Create a `.vh` file (e.g., `microcode_params.vh`) containing `localparam` definitions that `IP/microcode.sv` can `include`.
            ```verilog
            // microcode_params.vh
            localparam JADR_WIDTH = 5;
            localparam VARSEL_WIDTH = 4;
            // ...
            localparam INSTR_WIDTH = JADR_WIDTH + VARSEL_WIDTH + ...;
            ```
        *   **Pass as Command-Line Arguments:** If `verilog_generator.c` is invoked separately, the calculated widths could be passed as arguments.
    *   **Generate Microcode ROM Data:**
        *   The `MCode` structs need to be serialized back into a packed binary format (e.g., a `.mem` file for `$readmemh` or `$readmemb`) that `IP/microcode.sv` can load into its ROM.
        *   The packing order and bit-widths must exactly match the `INSTR_WIDTH` and unpacking logic in `IP/microcode.sv`. The compiler will pack the `MCode` struct fields into a single `uintX_t` (e.g., `uint64_t` if `INSTR_WIDTH` exceeds 32 bits) based on the *calculated* bit-widths.

2.  **Update `verilog_generator.c`:**
    *   Modify `verilog_generator.c` to:
        *   Read the dynamically generated bit-width parameters (e.g., from `microcode_params.vh` or command-line arguments).
        *   Instantiate `IP/microcode.sv` with these parameters.
        *   Ensure the ROM initialization in the generated Verilog matches the output format from `microcode_output.c`.

### Phase 5: Testing and Validation

1.  **Unit Tests:** Develop unit tests for `encode_compact_instruction` (now `create_mcode_instruction` or similar) to ensure fields are correctly populated and `max_val`s are tracked.
2.  **Integration Tests:** Test the entire pipeline: C code compilation -> microcode generation with dynamic widths -> Verilog generation -> hardware simulation.
3.  **Regression Testing:** Use existing test cases (e.g., in `test/`) to verify that the new pipeline produces functionally equivalent results. Pay close attention to edge cases for field sizes (e.g., a program requiring only 1 state, or a very long program pushing `jadr` limits).

### Considerations and Potential Challenges

*   **Bit Ordering:** Maintain strict consistency on bit ordering (MSB to LSB) when packing and unpacking fields between C and SystemVerilog.
*   **Padding/Alignment:** If `INSTR_WIDTH` doesn't align to 32 or 64 bits, consider if padding is necessary for memory efficiency or hardware simplicity.
*   **Error Handling:** Implement robust error checking for cases where a program demands more resources (states, memory addresses) than the hardware can physically support, even with parameterization.
*   **Backward Compatibility:** If this is an iterative development, consider how to maintain compatibility with older, fixed-width microcode if necessary.
*   **Tooling:** Ensure the build system (Makefile) correctly orchestrates the C compilation, parameter generation, and Verilog generation steps.

---

## Example from `hotstate/src/output.c` and Migration Strategy

The `hotstate` project's `home/phil/devel/FPGA/hotstate/src/output.c` provides a concrete example of how to implement the `MCode`-based approach, specifically demonstrating **Phase 4: Output Generation and Toolchain Integration**.

### Key Aspects of `hotstate/src/output.c` for Our Migration

1.  **`uCode` Array as Input:** The `PrintVivado` function takes `uCode *image` as its primary input. This `image` is an array of `uCode` structs, representing the entire microcode program. This directly aligns with our `CompactMicrocode` containing an array of `Code` structs.

2.  **Accessing Individual Fields:** `output.c` accesses individual microcode fields using the `image[i].uword.code.<field_name>` syntax (e.g., `image[i].uword.code.state`, `image[i].uword.code.jadr`). This highlights the readability and maintainability benefits of the struct-based approach over raw bitwise operations.

3.  **Dynamic Bit-Width Calculation:**
    *   `output.c` dynamically calculates the required bit-widths for various fields (e.g., `numStates`, `gAddrbits`, `varSelbits`, `Timers`, `gSwitches`) based on global variables populated during the earlier compilation phases.
    *   These calculated widths are then used to determine the `smdata_bits` (total instruction width) and to create bitmasks for packing.

4.  **Packing into `uint64_t` for `smdata`:**
    *   The core of `output.c`'s packing logic is found around lines 97-110. Here, individual fields from the `MCode` struct are bit-shifted and OR-ed together into a `uint64_t` array called `smdata`. This `smdata` array represents the final packed microcode words that will be loaded into the hardware ROM.
    *   The use of `uint64_t` demonstrates that the final packed instruction word can be wider than 32 bits if the sum of all field widths exceeds that.

5.  **Verilog Parameter Generation:**
    *   `PrintVivado` generates the instantiation of the `hotstate` Verilog module. Crucially, it overrides the module's `parameter` values (e.g., `NUM_STATES`, `NUM_ADR_BITS`, `NUM_VARSEL_BITS`, `NUM_TIMERS`, `NUM_SWITCHES`) with the dynamically calculated values from the C compilation. This is the mechanism by which the hardware becomes parameterized based on the specific program's requirements.

6.  **Memory File Generation:**
    *   `output.c` generates `.mem` files (e.g., `smdata.mem`, `vardata.mem`) containing the packed microcode and other data. These files can then be loaded by the Verilog testbench or directly by the hardware's ROMs (using `$readmemh`).

### Migration Strategy Inspired by `hotstate/src/output.c`

To migrate our project using insights from `hotstate/src/output.c`, we should focus on the following:

1.  **Centralize Program Metadata:**
    *   Ensure our C compiler (specifically the `CompactMicrocode` struct) collects and exposes the maximum values needed for all microcode fields (e.g., `max_jadr_val`, `max_varsel_val`, `max_state_val`). These will correspond to `gAddrbits`, `varSelbits`, `numStates`, etc., in the `hotstate` example.
    *   These values will be accessible to our `microcode_output.c` module.

2.  **Enhance `microcode_output.c` (Our Equivalent to `output.c`):**
    *   **Input:** It will take our `CompactMicrocode` struct (containing the `Code` array of `MCode` structs and the `max_val` fields) as input.
    *   **Bit-Width Calculation:** It will calculate the final bit-widths for each microcode field (e.g., `JADR_WIDTH = ceil(log2(max_jadr_val + 1))`) based on the collected metadata. It will also calculate the total `INSTR_WIDTH`.
    *   **Packing Logic:** Implement the bit-shifting and OR-ing logic to pack each `MCode` struct into a single `uintX_t` word (e.g., `uint64_t`) according to the dynamically calculated bit-widths and a defined bit-ordering. This is the direct equivalent of `smdata[i] |= (uint64_t)(image[i].uword.code.mask &statemask)<<numStates;` lines in `hotstate`'s `output.c`.
    *   **Verilog Parameter Header Generation:** Dynamically generate a Verilog header file (e.g., `microcode_params.vh`) containing `localparam` definitions for all the calculated bit-widths and the total `INSTR_WIDTH`. This file will be `include`d by our `IP/microcode.sv` module.
    *   **Microcode ROM Data Generation:** Write the packed `uintX_t` words to a `.mem` file, suitable for `$readmemh` in Verilog.
    *   **Testbench/Top-Level Verilog Generation:** Generate the top-level Verilog file that instantiates our parameterized `IP/microcode.sv` and passes the calculated parameters.

3.  **Parameterize Our `IP/microcode.sv`:**
    *   As outlined in Phase 3 of the overall plan, modify `IP/microcode.sv` to accept `parameter` values for all microcode field widths. The `hotstate` example's `hotstate #(` instantiation demonstrates this perfectly.

4.  **Integrate into Build System:**
    *   Ensure our `Makefile` (or build script) orchestrates the execution:
        *   C compilation to generate `CompactMicrocode` and populate `max_val`s.
        *   Execution of `microcode_output.c` to generate `microcode_params.vh` and `.mem` files.
        *   Verilog compilation, including the generated `microcode_params.vh`.

By following this strategy, we can leverage the proven approach of the `hotstate` project to achieve a flexible, dynamically sized, and parameterized microcode generation pipeline.
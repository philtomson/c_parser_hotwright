# Hotstate Code Overview

This document summarizes the roles, responsibilities, and dataflow of the original Hotstate project (from hotwright.com) `. It is structured to mirror `docs/code_overview.md` in this `c_parser_hotwright` repo to make cross-referencing straightforward.

## Conceptual Pipeline

The Hotstate project implements a hardware-oriented, microcoded execution engine and its toolchain.

High-level flow:

Source (C-like / examples)
→ Lexing + Parsing (flex/bison)
→ Linked-list-based IR (Struct.h + *List.c)
→ Program analysis and CFG/state derivation
→ Microcode / state machine generation
→ HDL/Verilog integration (Hotstate IP core)
→ Examples / FPGA builds / simulations

Compared to `c_parser_hotwright`:

- Hotstate:
  - Original implementation with implicit, global, linked-list IR.
  - Multiple ad hoc tools and generators spread across many C/C++ files.
- c_parser_hotwright:
  - Structured reimplementation:
    - Explicit AST, CFG, SSA.
    - Clear microcode representation and Verilog generation pipeline.
  - Aims to reproduce and extend Hotstate semantics.

## Repository-Level Structure (Hotstate)

Key top-level areas:

- `src/`
  - Core parser, list-based IR structures, analysis, and microcode/state generation logic.
- `IP/`
  - Verilog/SystemVerilog implementation of the Hotstate microcode engine and supporting blocks.
- `lib/`
  - Host-side libraries (`libhot`, `libkria`) to interact with the Hotstate hardware/microcode.
- `examples/`, `test_examples/`
  - End-to-end example designs (C input → microcode → FPGA/Verilator).
- `bin/`
  - Built tools and utilities (`hotstate`, `parse_c`, etc.).
- `include/`
  - Public includes for hardware/software integration (e.g. `hot.h`, `kria.h`).

The remainder of this document focuses on `src/`.

## Frontend: Lexing and Parsing

Hotstate’s frontend is built with flex/bison and constructs a global linked-list IR.

Key components:

- [`src/scan.l`](../hotstate/src/scan.l) and generated [`src/scan.c`](../hotstate/src/scan.c)
  - Role:
    - Lexical analyzer definition and implementation.
  - Responsibilities:
    - Tokenize the input source into identifiers, keywords, literals, operators, etc.
  - Functionality:
    - Feeds tokens into the bison-generated parser.

- [`src/gram.y`](../hotstate/src/gram.y), [`src/gram.tab.c`](../hotstate/src/gram.tab.c), [`src/gram.tab.h`](../hotstate/src/gram.tab.h), [`src/gram.c`](../hotstate/src/gram.c)
  - Role:
    - Bison grammar for the supported C-like language subset.
  - Responsibilities:
    - Define productions for functions, statements, expressions, control flow constructs.
    - Build *linked lists* and global structures as it parses.
  - Functionality:
    - Instead of constructing a typed AST, the grammar actions directly populate global lists (e.g., via `Struct.h`), using a global "main token" structure (`gMT`) to root the IR.

- [`src/Struct.h`](../hotstate/src/Struct.h), [`src/moreTypes.h`](../hotstate/src/moreTypes.h), [`src/config.h`](../hotstate/src/config.h), [`src/version.h`](../hotstate/src/version.h)
  - Role:
    - Central type and configuration definitions.
  - Responsibilities:
    - Define:
      - Nodes for functions, variables, statements, expressions, switches, loops, states, operations, etc.
      - Global aggregates and configuration flags.
  - Functionality:
    - Provide the implicit IR schema used throughout the codebase.

- [`src/getopt.c`](../hotstate/src/getopt.c), [`src/getopt.h`](../hotstate/src/getopt.h)
  - Role:
    - Local CLI option parsing.
  - Functionality:
    - Support command-line interfaces of Hotstate tools.

- [`src/Readme.txt`](../hotstate/src/Readme.txt)
  - Notes:
    - Documents build/debug usage and indicates that `gram.y` builds linked lists describing the program, traversed via global structures (notably `gMT`).

## IR Construction: Linked-List Modules

Instead of AST/CFG structs, Hotstate models nearly everything as typed linked lists.

Representative files (each implements one list/IR dimension):

- [`src/funcList.c`](../hotstate/src/funcList.c), [`src/funcPro.h`](../hotstate/src/funcPro.h)
  - Functions and prototypes.

- [`src/varList.c`](../hotstate/src/varList.c), [`src/varPro.h`](../hotstate/src/varPro.h)
  - Variables.

- [`src/typedefList.c`](../hotstate/src/typedefList.c`
  - Typedefs and type aliases.

- [`src/stmtList.c`](../hotstate/src/stmtList.c)
  - Statements (if/while/for/switch/goto/return/blocks).

- [`src/exprList.c`](../hotstate/src/exprList.c)
  - Expressions.

- [`src/ctlList.c`](../hotstate/src/ctlList.c)
  - Control transfer constructs.

- [`src/loopJumpList.c`](../hotstate/src/loopJumpList.c)
  - Loop jumps, `break`/`continue` style information.

- [`src/switchList.c`](../hotstate/src/switchList.c)
  - Switch-case constructs and their targets.

- [`src/labelList.c`](../hotstate/src/labelList.c)
  - Labels and `goto` destinations.

- [`src/threadList.c`](../hotstate/src/threadList.c)
  - Thread or concurrent operation lists (where applicable).

- [`src/assignList.c`](../hotstate/src/assignList.c)
  - Assignments.

- [`src/opList.c`](../hotstate/src/opList.c)
  - Operation/micro-op style records used in later codegen.

- [`src/simOpList.c`](../hotstate/src/simOpList.c)
  - Simulation-oriented op records.

- [`src/stateList.c`](../hotstate/src/stateList.c)
  - Microcode state / state-machine entries.

- [`src/stack.c`](../hotstate/src/stack.c), [`src/stack.h`](../hotstate/src/stack.h)
  - Stack structures used during parsing/analysis/codegen.

Each:

- Role:
  - Encapsulate creation, linking, and traversal of a specific entity type.
- Responsibilities:
  - Called by the parser (`gram.y`) to populate global IR.
  - Used by analysis and backend code to walk and transform the program.
- Functionality:
  - Together form the implicit IR that Hotstate operates on.
  - This corresponds to:
    - `ast.[ch]`, `cfg.[ch]`, `cfg_builder.[ch]`, etc. in the new repo, but organized as lists rather than explicit trees/graphs.

## Analysis and CFG/State Derivation

These modules consume the list-based IR to infer control flow, branches, and states.

- [`src/analysis.c`](../hotstate/src/analysis.c)
  - Role:
    - High-level analysis over parsed lists.
  - Responsibilities:
    - Inspect functions/statements/branches.
    - Compute information needed for state machine generation (e.g., structure, dependencies).
  - Functionality:
    - Conceptually similar to a mix of `cfg_builder`, `hw_analyzer`, and parts of `ssa_optimizer` in `c_parser_hotwright`.

- CFG/branch-related tools:
  - [`src/branch_analyzer`](../hotstate/src/branch_analyzer),
    [`src/branch_analyzer.cpp`](../hotstate/src/branch_analyzer.cpp),
    [`src/branch_analyzer.ll`](../hotstate/src/branch_analyzer.ll)
  - [`src/cfg_builder`](../hotstate/src/cfg_builder),
    [`src/cfg_builder.cpp`](../hotstate/src/cfg_builder.cpp),
    [`src/cfg_builder_claude.cpp`](../hotstate/src/cfg_builder_claude.cpp),
    [`src/cfg_builder_gemini.cpp`](../hotstate/src/cfg_builder_gemini.cpp),
    [`src/deep_cfg_builder.cpp`](../hotstate/src/deep_cfg_builder.cpp),
    [`src/cfg_extractor.cpp`](../hotstate/src/cfg_extractor.cpp)
  - Role:
    - Investigate and construct control-flow information and alternative CFG-like representations.
  - Responsibilities:
    - Map the global lists into more explicit CFG/state models.
    - Provide debug/visualization/experimental pipelines.
  - Functionality:
    - Rough precursors to the explicit `cfg_*` infrastructure in the new repo.

- [`src/bitpattern_switch.c`](../hotstate/src/bitpattern_switch.c)
  - Role:
    - Implement bit-pattern-oriented lowering for `switch` constructs.
  - Responsibilities:
    - Translate switch/case sets into microcode-efficient patterns.
  - Functionality:
    - Hardware-aware encoding strategy mirrored/extended by `c_parser_hotwright`.

## Microcode and Output Generation

These modules traverse the IR and analyses to generate the concrete microcode/state machine.

- [`src/output.c`](../hotstate/src/output.c),
  [`src/output_new.c`](../hotstate/src/output_new.c),
  [`src/output_orig.c`](../hotstate/src/output_orig.c)
  - Role:
    - Microcode / state table generator(s).
  - Responsibilities:
    - Assign state IDs and next-state relationships.
    - Emit micro-instructions and tables in formats consumed by the Hotstate IP.
    - Provide multiple implementations or iterations (orig vs new).
  - Functionality:
    - Corresponds to:
      - `cfg_to_microcode.c`, `ast_to_microcode.c`, and `microcode_output.c` in `c_parser_hotwright`.

- [`src/ram2mf.c`](../hotstate/src/ram2mf.c)
  - Role:
    - Convert RAM initialization or microcode tables to `.mem`/tool-specific formats.
  - Functionality:
    - Ancillary backend, similar to microcode artifact writers in the new repo.

- [`src/hdl.c`](../hotstate/src/hdl.c)
  - Role:
    - HDL/Verilog integration utility.
  - Responsibilities:
    - Generate or glue together HDL code for the microcode/state machine.
  - Functionality:
    - Predecessor to `verilog_generator.c` in the structured toolchain.

## Tools, Integrations, and Utilities

- [`src/main.c`](../hotstate/src/main.c)
  - Role:
    - Primary entry point.
  - Responsibilities:
    - Parse CLI options.
    - Invoke lexing/parsing.
    - Call analysis and output/microcode generation routines.
  - Functionality:
    - Analogous to `src/main.c` in `c_parser_hotwright`.

- [`src/parse_c`](../hotstate/src/parse_c),
  [`src/parse_c_code`](../hotstate/src/parse_c_code),
  [`src/parse_c.cpp`](../hotstate/src/parse_c.cpp)
  - Role:
    - Parsing utilities / frontends.
  - Functionality:
    - Provide various pipelines into the list-based IR.

- [`src/llvm_parse.cpp`](../hotstate/src/llvm_parse.cpp),
  [`src/py_clang.py`](../hotstate/src/py_clang.py),
  [`src/pycparser_c.py`](../hotstate/src/pycparser_c.py)
  - Role:
    - Alternate frontends (LLVM, Python) for experimentation/analysis.
  - Functionality:
    - Show how Hotstate’s IR can be driven from different parsing ecosystems.

- Temporary/example files:
  - `example.c`, `example2.c`, `*.ll`, `*.plist`, `example_opt.ll`, `tmp`, `out`, etc.
  - Role:
    - Test inputs and intermediate outputs for development.

- `errors`, `errorsls`
  - Role:
    - Error log outputs.

## Hardware / IP Layer (High-Level)

While not under `src/`, these are essential to understanding the Hotstate end-to-end flow:

- `IP/hotstate.sv`, `IP/microcode.sv`, `IP/stack.sv`, `IP/next_address.sv`,
  `IP/timer.sv`, `IP/variable.sv`, `IP/switch.sv`, `IP/control.sv`,
  `IP/misc/*.v`
  - Role:
    - RTL implementation of the Hotstate microcoded engine and its primitives.
  - Responsibilities:
    - Execute the generated microcode/state tables.
  - Functionality:
    - Consumes outputs from `src/output*.c` and related tools.

- `lib/hotlib.c`, `lib/krialib.c`, `libhot.so`, `libkria.so`
  - Role:
    - Host libraries for interacting with Hotstate hardware/microcode.
  - Functionality:
    - Provide APIs for loading microcode, controlling execution, and I/O.

- `examples/`, `test_examples/`
  - Role:
    - Demonstrate full flows: from C-like descriptions through Hotstate to real hardware/simulation.
  - Functionality:
    - Serve as behavioral references for `c_parser_hotwright` parity and regression.

## End-to-End Dataflow (Concise)

1. Frontend:
   - `scan.l`/`scan.c` + `gram.y`/`gram.tab.c` parse input.
   - Grammar actions populate global linked-list IR (as defined in `Struct.h`, etc.).

2. IR & Analysis:
   - `*List.c` modules maintain lists for all program entities.
   - `analysis.c`, `branch_analyzer*`, `cfg_builder*`, `deep_cfg_builder.cpp`, `cfg_extractor.cpp` interpret control flow and structure.

3. Microcode / States:
   - `output.c` / `output_new.c` / `bitpattern_switch.c` map analysis results to:
     - Enumerated states.
     - Micro-instruction encodings.
     - Transition tables.

4. HDL / Deployment:
   - `hdl.c`, `ram2mf.c` and IP/*.sv combine:
     - Microcode/state tables.
     - Hotstate core RTL.
   - Output is used in FPGA projects under `examples/` and beyond.

5. Runtime:
   - `lib/` libraries plus bin tools manage loading and driving the Hotstate system.

## Mapping to c_parser_hotwright

For cross-project orientation:

- Hotstate’s:
  - `scan.l`/`gram.y` + linked lists
  - analysis + branch/cfg builders
  - output + hdl + ram2mf

map to c_parser_hotwright’s:

- `lexer.c` / `parser.c` / `ast.[ch]`
- `cfg.[ch]`, `cfg_builder.[ch]`, `cfg_utils.[ch]`, `ssa_optimizer.[ch]`, `hw_analyzer.[ch]`
- `microcode.[ch]`, `ast_to_microcode.[ch]`, `cfg_to_microcode.[ch]`, `microcode_output.c`, `verilog_generator.[ch]`

c_parser_hotwright preserves Hotstate’s semantics and hardware model while:

- Making the IR explicit (AST/CFG/SSA).
- Structuring passes cleanly.
- Enabling more robust extensions and analysis than the original, monolithic linked-list design.
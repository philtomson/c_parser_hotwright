# Code Overview

This document summarizes the roles, responsibilities, and dataflow of the core modules under `src/` for the Hotstate-compatible C-to-microcode toolchain.

At a high level, the pipeline is:

C source → Preprocessor → Lexer → Parser → AST → CFG (+ SSA/analysis) → Microcode → Verilog/outputs

## Frontend: From C source to AST

- [`src/preprocessor.c`](src/preprocessor.c)
  - Role: Lightweight C preprocessor.
  - Responsibilities:
    - Handle `#include`, `#define`, and selected conditional compilation constructs.
    - Normalize input into a single logical source stream.
  - Functionality:
    - Produces preprocessed source text, which is then fed into the lexer.

- [`src/lexer.c`](src/lexer.c)
  - Role: Lexical analysis.
  - Responsibilities:
    - Convert characters into tokens (identifiers, keywords, literals, operators, punctuation).
    - Track locations for precise diagnostics.
  - Functionality:
    - Implements the lexical rules for the supported C subset.
    - Serves tokens to the parser.

- [`src/lexer.h`](src/lexer.h)
  - Declares token types, lexer interface, and supporting structures.

- [`src/parser.c`](src/parser.c)
  - Role: Syntactic analysis.
  - Responsibilities:
    - Consume tokens from the lexer and build an abstract syntax tree (AST).
    - Enforce grammar rules for the supported C subset.
    - Report syntax errors and perform limited recovery where possible.
  - Functionality:
    - Central frontend component that transforms tokens into structured AST nodes defined in `ast.h`.

- [`src/parser.h`](src/parser.h)
  - Declares the parser entry points and parser context structures.

- [`src/ast.h`](src/ast.h) / [`src/ast.c`](src/ast.c)
  - Role: AST definition and helpers.
  - Responsibilities:
    - Define node types for:
      - Declarations, functions, statements (if/while/for/switch/goto/return), expressions, etc.
    - Provide constructors, utility routines, and possibly printing/inspection helpers.
  - Functionality:
    - Canonical in-memory representation shared by all downstream stages.

## Intermediate Representation: CFG and utilities

- [`src/cfg.h`](src/cfg.h) / [`src/cfg.c`](src/cfg.c)
  - Role: Core control-flow graph (CFG) and SSA-oriented IR.
  - Responsibilities:
    - Define:
      - Instruction/operation enums.
      - `SSAValue`, SSA instructions, basic blocks, and `CFG` container.
    - Provide functions to manage blocks, instructions, and graph topology.
  - Functionality:
    - Primary IR used for analysis, optimization, and microcode lowering.

- [`src/cfg_builder.h`](src/cfg_builder.h) / [`src/cfg_builder.c`](src/cfg_builder.c)
  - Role: AST → CFG construction.
  - Responsibilities:
    - Traverse the AST and emit CFG for:
      - Sequencing, conditionals, loops, gotos, switches, functions, returns.
    - Manage:
      - Pending gotos and labels.
      - Loop and switch contexts for `break`/`continue`.
      - Initial variable versioning scaffolding.
  - Functionality:
    - Encodes high-level structured control flow into explicit CFG edges and blocks.

- [`src/cfg_utils.h`](src/cfg_utils.h) / [`src/cfg_utils.c`](src/cfg_utils.c)
  - Role: CFG support utilities.
  - Responsibilities:
    - Common helpers for:
      - Traversal and printing.
      - Computing predecessors/successors and basic properties.
      - Validation and debugging views.
  - Functionality:
    - Shared infrastructure used by tests, analyzers, optimizers, and codegen.

- [`src/test_cfg.c`](src/test_cfg.c)
  - Role: CFG construction tests/examples.
  - Responsibilities:
    - Contain small programs and drivers to assert correctness of AST→CFG.
  - Functionality:
    - Developer-facing; not part of the core compilation pipeline.

## Optimization and Analysis

- [`src/ssa_optimizer.h`](src/ssa_optimizer.h) / [`src/ssa_optimizer.c`](src/ssa_optimizer.c)
  - Role: SSA transformation and optimization over CFG.
  - Responsibilities:
    - Convert CFG into SSA form (inserting phi nodes, managing variable versions).
    - Apply targeted optimizations appropriate for microcode/hardware generation:
      - Constant propagation/folding.
      - Dead code elimination and related cleanups.
  - Functionality:
    - Produces a simplified IR that is easier to map to efficient microcode.

- [`src/expression_evaluator.h`](src/expression_evaluator.h) / [`src/expression_evaluator.c`](src/expression_evaluator.c)
  - Role: Expression-level evaluation and simplification.
  - Responsibilities:
    - Evaluate constant expressions.
    - Provide helpers to reason about values/types/widths where statically known.
  - Functionality:
    - Used by SSA optimizer, CFG builder, and/or microcode generators to reduce complexity and bake in constants.

- [`src/hw_analyzer.h`](src/hw_analyzer.h) / [`src/hw_analyzer.c`](src/hw_analyzer.c)
  - Role: Hardware-oriented program analysis.
  - Responsibilities:
    - Analyze IR (AST/CFG/SSA) to derive hardware parameters:
      - Required state counts, variable lifetimes, storage needs, etc.
    - Construct a hardware context object consumed by microcode and Verilog generators.
  - Functionality:
    - Ensures that generated microcode/RTL adheres to architectural/resource constraints of the Hotstate-style engine.

## Microcode Representation and Generation

- [`src/microcode_defs.h`](src/microcode_defs.h)
  - Role: Microcode encoding specification.
  - Responsibilities:
    - Define structural constants and helper types used during microcode creation.
  - Functionality:
    - Shared low-level definitions referenced by microcode-related modules.

- [`src/microcode.h`](src/microcode.h)
  - Role: In-memory microcode representation.
  - Responsibilities:
    - Define core micro-instruction formats, fields, and related data structures.
    - Represent states, transitions, and control signals.
  - Functionality:
    - Contract between IR-based lowering and concrete serialized microcode artifacts.

- [`src/ast_to_microcode.h`](src/ast_to_microcode.h) / [`src/ast_to_microcode.c`](src/ast_to_microcode.c)
  - Role: Direct AST → microcode lowering.
  - Responsibilities:
    - Walk AST to emit sequences of “compact microcode” instructions.
    - Implement structured control constructs:
      - If/else, loops, switches, break/continue/goto in terms of microcode addresses.
    - Manage loop/switch context stacks and backpatch addresses.
  - Functionality:
    - Specialized path to microcode that may bypass full CFG/SSA when appropriate, tuned to the Hotstate microarchitecture.

- [`src/cfg_to_microcode.h`](src/cfg_to_microcode.h) / [`src/cfg_to_microcode.c`](src/cfg_to_microcode.c)
  - Role: CFG/SSA → microcode lowering.
  - Responsibilities:
    - Translate basic blocks and SSA instructions into micro-instructions.
    - Assign state indices/addresses and encode control flow, branches, and merges.
    - Incorporate `hw_analyzer` output (e.g., resource/state limits) into layout choices.
  - Functionality:
    - Main structured backend that converts analyzed IR into a microcode program.

- [`src/microcode_output.c`](src/microcode_output.c)
  - Role: Microcode artifact emission.
  - Responsibilities:
    - Serialize microcode into external formats:
      - `.mem`/switchdata, lookup tables, or other files consumed by hardware/sim.
    - Compute bit widths and layout consistent with `microcode_defs` and hardware design.
  - Functionality:
    - Bridge between in-memory representation and hardware/test infrastructure.

## Verilog / Hardware Integration

- [`src/verilog_generator.h`](src/verilog_generator.h) / [`src/verilog_generator.c`](src/verilog_generator.c)
  - Role: Verilog/RTL generator.
  - Responsibilities:
    - Generate Verilog modules reflecting:
      - Microcoded control.
      - Integration hooks for memories and datapath as dictated by microcode.
    - Parameterize generated RTL with analysis results and microcode layout.
  - Functionality:
    - Allows automatic construction of Hotstate-style hardware instances for a given program.

- [`src/verilator_sim.h`](src/verilator_sim.h)
  - Role: Verilator integration.
  - Responsibilities:
    - Provide declarations and interfaces needed to drive the generated Verilog in Verilator-based simulations.
  - Functionality:
    - Support infrastructure for simulation, regression tests, and CI.

## Entry Point

- [`src/main.c`](src/main.c)
  - Role: CLI front-end and orchestration.
  - Responsibilities:
    - Parse command-line arguments and configuration.
    - Run the pipeline:
      1. Preprocess and lex the input.
      2. Parse into an AST.
      3. Optionally build CFG and SSA; run hardware analysis.
      4. Generate microcode (via `ast_to_microcode` or `cfg_to_microcode`).
      5. Emit microcode files and, if requested, generate Verilog.
  - Functionality:
    - Wires all core components into a single command-line tool.

## End-to-End Dataflow (Concise)

1. `main.c`:
   - Reads input C and options.
2. Frontend:
   - `preprocessor.c` → `lexer.c` → `parser.c` → `ast.c`/`ast.h`: produce AST.
3. Middle-end:
   - Option A: `ast_to_microcode.c` directly lowers AST to microcode.
   - Option B:
     - `cfg_builder.c` builds CFG (`cfg.c`/`cfg.h`).
     - `cfg_utils.c` supports analysis/inspection.
     - `ssa_optimizer.c` (+ `expression_evaluator.c`) optimize in SSA form.
     - `hw_analyzer.c` derives a hardware context.
     - `cfg_to_microcode.c` lowers CFG/SSA to microcode.
4. Backend:
   - `microcode_output.c` emits microcode artifacts.
   - `verilog_generator.c` (+ `verilator_sim.h`) generates and integrates Verilog/RTL for Hotstate-style execution.

This separation cleanly distinguishes:
- Frontend language understanding.
- IR-based analysis and optimization.
- Backend microcode and hardware generation.
# Control Flow Graph (CFG) Implementation

This document describes the Control Flow Graph (CFG) implementation added to the C parser project.

## Overview

The CFG implementation converts the Abstract Syntax Tree (AST) into a Control Flow Graph representation, which is an intermediate representation showing all paths that might be traversed through a program during its execution. This forms the foundation for future optimizations and code generation.

## Features

### Current Implementation

1. **Basic CFG Construction**
   - Converts AST to CFG with basic blocks
   - Each basic block contains SSA-style instructions
   - Proper edge connections between blocks

2. **Supported Language Constructs**
   - Function definitions
   - Variable declarations and assignments
   - Arithmetic expressions
   - If-else statements
   - While loops
   - For loops
   - Return statements
   - Function calls

3. **SSA-Style Instructions**
   - Variable versioning (e.g., x_1, x_2)
   - Temporary variables for intermediate results
   - Explicit control flow (jumps and branches)

4. **Visualization**
   - DOT file generation for Graphviz
   - Automatic PNG generation
   - Clear labeling of blocks and edges

## Architecture

### Key Components

1. **cfg.h/cfg.c** - Core data structures
   - `CFG` - Control flow graph
   - `BasicBlock` - Basic block with instructions
   - `SSAValue` - SSA values (variables, constants, temporaries)
   - `SSAInstruction` - Individual instructions

2. **cfg_builder.h/cfg_builder.c** - AST to CFG conversion
   - `build_cfg_from_ast()` - Main entry point
   - Statement and expression processors
   - Variable version tracking

3. **cfg_utils.h/cfg_utils.c** - Utilities
   - `cfg_to_dot()` - Generate DOT files
   - `print_cfg()` - Pretty printing
   - `verify_cfg()` - CFG verification

## Usage

### Building CFG from AST

```c
// Parse C code
Lexer* lexer = lexer_create(source_code);
// ... tokenize ...
Parser* parser = parser_create(tokens, token_count);
Node* ast = parse(parser);

// Build CFG
CFG* cfg = build_cfg_from_ast(ast);

// Generate visualization
cfg_to_dot(cfg, "output.dot");

// Clean up
free_cfg(cfg);
```

### Example Output

For this C code:
```c
int max(int a, int b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}
```

The CFG contains:
- **Block 0 (entry)**: Evaluates condition `a > b`
- **Block 1 (exit)**: Function exit point
- **Block 2 (if.then)**: Returns `a`
- **Block 3 (if.else)**: Returns `b`

## Future Enhancements

The design document (`docs/cfg_ssa_design.md`) outlines future enhancements including:

1. **Full SSA Form**
   - Phi functions at join points
   - Dominance analysis
   - Dominance frontier calculation

2. **Optimizations**
   - Constant propagation
   - Dead code elimination
   - Common subexpression elimination
   - Global value numbering

3. **Advanced Features**
   - Memory SSA for pointer analysis
   - Loop optimizations
   - Inlining support

4. **Code Generation**
   - Backend for different architectures
   - Register allocation
   - Instruction selection

## Testing

Run the test suite:
```bash
make test_cfg
./test_cfg
```

Generate visualizations:
```bash
make graphs
```

This will create PNG files for each test case showing the CFG structure.

## Files Added

- `cfg.h` - CFG data structure definitions
- `cfg.c` - CFG implementation
- `cfg_builder.h` - CFG builder interface
- `cfg_builder.c` - AST to CFG conversion
- `cfg_utils.h` - CFG utilities interface
- `cfg_utils.c` - Visualization and debugging utilities
- `test_cfg.c` - Test suite for CFG functionality
- `docs/cfg_ssa_design.md` - Detailed design document with SSA form

## Integration

The CFG implementation integrates seamlessly with the existing parser:
- Uses the same AST nodes
- Reuses token types for operators
- Maintains compatibility with existing code

The modular design allows for easy extension and modification without affecting the parser core.
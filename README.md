# C Parser with Control Flow Graph Generation

A recursive descent C parser that builds an Abstract Syntax Tree (AST) and generates Control Flow Graphs (CFG) for program analysis and optimization.

## Features

### Parser Features
- **Lexical Analysis**: Tokenizes C source code
- **Recursive Descent Parsing**: Builds AST from tokens
- **Supported Language Constructs**:
  - Variable declarations (`int x = 5;`)
  - Arithmetic expressions with proper precedence
  - Control structures:
    - If-else statements
    - While loops
    - For loops
    - Switch statements (partial)
  - Functions:
    - Function definitions with parameters
    - Function calls with arguments
    - Return statements
  - Comments (single-line `//` and multi-line `/* */`)
  - Break statements

### Control Flow Graph (CFG) Features
- **AST to CFG Conversion**: Transforms the AST into a graph-based representation
- **SSA-Style Instructions**: Uses variable versioning for analysis
- **Visualization**: Generates DOT files for Graphviz rendering
- **Foundation for Optimization**: Ready for SSA form and optimization passes

## Building

```bash
make clean
make all
```

This builds:
- `c_parser` - Main parser executable
- `test_cfg` - CFG test suite

## Usage

### Basic Parser Usage

```bash
# Parse a file
./c_parser test/test_simple.c

# Parse default example
./c_parser
```

### CFG Generation

```bash
# Run CFG tests
./test_cfg

# Generate visualization graphs
make graphs
```

This creates DOT files and PNG images showing the control flow graphs.

## Project Structure

```
├── lexer.h/c          # Lexical analyzer
├── parser.h/c         # Recursive descent parser
├── ast.h/c            # AST node definitions
├── cfg.h/c            # CFG data structures
├── cfg_builder.h/c    # AST to CFG conversion
├── cfg_utils.h/c      # CFG utilities and visualization
├── main.c             # Main program
├── test_cfg.c         # CFG test suite
├── test/              # Test C programs
└── docs/              # Documentation
    └── cfg_ssa_design.md  # Detailed CFG/SSA design
```

## Example

Input C code:
```c
int max(int a, int b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}
```

Generated CFG:
- Entry block evaluates `a > b`
- True branch returns `a`
- False branch returns `b`
- All paths lead to exit block

## Testing

Run all tests:
```bash
# Parser tests
./c_parser test/test_functions_complete.c

# CFG tests with visualization
make test
make graphs
```

## Future Work

See `docs/cfg_ssa_design.md` for planned enhancements:
- Full Static Single Assignment (SSA) form with phi functions
- Optimization passes (constant propagation, dead code elimination)
- Code generation backends
- Advanced analyses (dominance, loop detection)

## Documentation

- `README_CFG.md` - Detailed CFG implementation documentation
- `docs/cfg_ssa_design.md` - Comprehensive design for SSA form and optimizations

## License

This project is for educational purposes.
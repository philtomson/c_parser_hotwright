# C Parser with Control Flow Graph Generation

A recursive descent C parser that builds an Abstract Syntax Tree (AST) and generates Control Flow Graphs (CFG) for program analysis and optimization.

## Features

### Parser Features
- **Lexical Analysis**: Tokenizes C source code with line/column tracking
- **Recursive Descent Parsing**: Builds AST from tokens
- **Enhanced Error Reporting**: Precise error messages with line and column numbers
- **Supported Language Constructs**:
  - Variable declarations (`int x = 5;`, `bool flag = true;`)
  - **C23 `_BitInt` Types**: Bit-precise integers with indexing (`_BitInt(8) x = {1,0,1,1,0,0,1,0};`)
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

### CFG Generation with DOT Output

```bash
# Generate CFG and DOT file for visualization
./c_parser --dot test/test_simple.c

# Enable debug output to see detailed parsing information
./c_parser --debug --dot test/test_simple.c

# Run CFG tests
./test_cfg

# Generate visualization graphs
make graphs
```

This creates DOT files and PNG images showing the control flow graphs.

### C23 `_BitInt` Support

The parser supports C23 `_BitInt` types with a unique bit-indexing feature:

```c
int main() {
    _BitInt(3) x = {1, 0, 1};  // x = 5 (binary 101)
    _BitInt(8) byte = 255;     // 8-bit integer
    
    if (x[1]) {                // Test bit 1
        x[2] = 0;              // Clear bit 2
    }
    
    byte[7] = x[0];            // Copy bit 0 of x to bit 7 of byte
    return 0;
}
```

See `docs/bitint_feature.md` for complete documentation.

### Enhanced Error Messages

The parser provides detailed error reporting with line and column information:

```bash
$ ./c_parser test_errors.c
Parse Error at line 2, column 14: _BitInt width must be positive
  Token: RPAREN (')')
```

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

## Developer Information

### Debug Output

The parser includes a centralized debug output system:

```c
// Use this function instead of printf for debug output
print_debug("DEBUG: Processing node type %d\n", node->type);

// The function automatically checks debug_mode and only prints when enabled
// Equivalent to:
// if (debug_mode) {
//     printf("DEBUG: Processing node type %d\n", node->type);
// }
```

**Benefits:**
- Cleaner code without scattered `if (debug_mode)` checks
- Centralized debug control
- Support for printf-style formatting with variable arguments
- Easy to enable/disable debug output globally

**Usage in Code:**
- Include `ast.h` to access the `print_debug()` function
- Replace `if (debug_mode) { printf(...); }` with `print_debug(...)`
- The function handles the debug mode check internally

## Documentation

- `README_CFG.md` - Detailed CFG implementation documentation
- `docs/cfg_ssa_design.md` - Comprehensive design for SSA form and optimizations
- `docs/bitint_feature.md` - Complete `_BitInt` type documentation with examples

## License

This project is for educational purposes.
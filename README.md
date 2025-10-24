# C Parser with support for Hotwright microcode generation

A recursive descent C parser that builds an Abstract Syntax Tree (AST), generates Control Flow Graphs (CFG), and generates hardware microcode and Verilog HDL for FPGA implementation.

## Features

### Parser Features
- **Lexical Analysis**: Tokenizes C source code with line/column tracking
- **Recursive Descent Parsing**: Builds AST from tokens
- **Enhanced Error Reporting**: Precise error messages with line and column numbers
- **Supported Language Constructs**:
  - Variable declarations (`int x = 5;`, `bool flag = true;`)
  - Arrays and initialization lists
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

### Hardware Synthesis Features
- **Hardware Analysis**: Detects state variables and input variables from C code
- **Microcode Generation**: Produces hotstate-compatible microcode for FPGA implementation
- **Verilog HDL Generation**: Creates synthesizable Verilog modules and testbenches
- **Memory File Generation**: Outputs .mem files for FPGA synthesis tools

## Building

```bash
cd src
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

### Hardware Synthesis Usage

```bash
# Analyze hardware constructs (state and input variables)
./c_parser --hardware ../test/test_hardware_local.c

# Generate hotstate-compatible microcode
./c_parser --microcode-hs ../test/test_hardware_local.c

# Generate Verilog HDL module
./c_parser --verilog ../test/test_hardware_local.c

# Generate Verilog testbench
./c_parser --testbench ../test/test_hardware_local.c

# Generate complete HDL package (module, testbench, stimulus, makefile)
./c_parser --all-hdl ../test/test_hardware_local.c

# Combine multiple options
./c_parser --hardware --microcode-hs --verilog test/test_hardware_local.c
```

#### Hardware Variable Patterns

The parser recognizes hardware patterns in C code:

```c
int main() {
    // State variables (outputs) - boolean with initialization
    bool LED0 = 0;  // Detected as state0
    bool LED1 = 0;  // Detected as state1
    bool LED2 = 1;  // Detected as state2
    
    // Input variables - boolean without initialization
    bool a0, a1, a2;  // Detected as input0, input1, input2
    
    // Hardware logic
    if (a0) {
        LED0 = 1;
    }
    
    if (a1) {
        LED1 = 1;
    }
    
    return 0;
}
```

#### Generated Output Files

- **`module_template.v`** - Synthesizable Verilog module
- **`module_tb.v`** - Verilog testbench
- **`module_smdata.mem`** - Microcode memory file
- **`module_vardata.mem`** - Variable mapping file
- **`user.v`** - User-editable stimulus file
- **`Makefile.sim`** - Simulation makefile

### C23 `_BitInt` Support

The parser supports C23 `_BitInt` types with additional bit-indexing feature:

```c
int main() {
    _BitInt(3) x = {1, 0, 1};  // x = 5 (binary 101)
    _BitInt(8) byte = 255;     // 8-bit integer
    
    if (x[1]) {                // Test bit 1 (NOT C23 compatible)
        x[2] = 0;              // Clear bit 2
    }
    
    byte[7] = x[0];            // Copy bit 0 of x to bit 7 of byte
    return 0;
}
```

See `docs/bitint_feature.md` for complete documentation.

### Error Messages

The parser provides detailed error reporting with line and column information:

```bash
$ ./c_parser test_errors.c
Parse Error at line 2, column 14: _BitInt width must be positive
  Token: RPAREN (')')
```

## Project Structure

```
/src
├── lexer.h/c              # Lexical analyzer
├── parser.h/c             # Recursive descent parser
├── ast.h/c                # AST node definitions
├── cfg.h/c                # CFG data structures
├── cfg_builder.h/c        # AST to CFG conversion
├── cfg_utils.h/c          # CFG utilities and visualization
├── hw_analyzer.h/c        # Hardware variable analysis
├── cfg_to_microcode.h/c   # CFG to microcode translation
├── microcode_output.c     # Microcode output generation
├── verilog_generator.h/c  # Verilog HDL generation
├── main.c                 # Main program
├── test_cfg.c             # CFG test suite
├── test/                  # Test C programs
│   └── test_hardware_local.c  # Hardware synthesis example
docs/                  # Documentation
    ├── cfg_ssa_design.md  # Detailed CFG/SSA design
    └── hardware_synthesis.md  # Hardware synthesis documentation
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

## Command Line Options

```bash
./c_parser [OPTIONS] <filename.c>

Options:
  --dot          Generate a DOT file for CFG visualization
  --debug        Enable debug output messages
  --hardware     Analyze hardware constructs (state/input variables)
  --microcode    Generate microcode from CFG
  --microcode-hs Generate hotstate-compatible microcode from the AST
  --verilog      Generate Verilog HDL module
  --testbench    Generate Verilog testbench
  --all-hdl      Generate all HDL files (module, testbench, stimulus, makefile)
```

## Future Work

See `docs/cfg_ssa_design.md` for planned enhancements:
- Static Single Assignment (SSA) intermediate form with phi functions
- Optimization passes (constant propagation, dead code elimination)
- Advanced hardware optimizations (state minimization)
- VHDL HDL generation
- Advanced analyses (dominance, loop detection)

## Developer Information

## Documentation

- `README_CFG.md` - Detailed CFG implementation documentation
- `docs/cfg_ssa_design.md` - Comprehensive design for SSA form and optimizations
- `docs/bitint_feature.md` - Complete `_BitInt` type documentation with examples
- `docs/hardware_synthesis.md` - Hardware synthesis and HDL generation guide

## License

This project is for educational purposes.

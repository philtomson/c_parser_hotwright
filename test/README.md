# C Parser Test Suite

This directory contains comprehensive tests for the c_parser, including tests for multiple variable declarations and `#include` functionality.

## Running Tests

### Quick Test Suite
```bash
make run_tests
```
Runs all tests with a summary report.

### Specific Functionality Tests
```bash
make test_multi_vars    # Test multiple variable declarations
make test_includes      # Test #include functionality
```

### Individual Test Files

#### Multiple Variable Declaration Tests
- **`test_multi_vars.c`** - Basic multiple variable declarations (`int a, b, c;`)
- **`test_multi_init.c`** - Multiple variables with mixed initialization (`int a = 1, b, c = 3;`)

#### Variable Scoping Tests
- **`test_scoping.c`** - Basic variable scoping and shadowing
- **`test_scoping_advanced.c`** - Advanced nested scoping with multiple levels

#### Include Functionality Tests
- **`test_include_multi.c`** - Tests `#include` with multiple variable declarations
- **`test_header_multi.h`** - Header file with multiple variable declarations
- **`test_include_final.c`** - Complete include test with logical operations
- **`test_logical_ops.c`** - Logical operations test (`if(a0 == 0 && a1 == 1)`)

#### Core Language Feature Tests
- **`test_while.c`** - While loop constructs
- **`test_if_while.c`** - Combined if-while statements
- **`test_functions.c`** - Function definitions and calls
- **`test_for.c`** - For loop constructs
- **`test_all_loops.c`** - Comprehensive loop testing

## Test Features

### Multiple Variable Declarations
The parser now supports:
```c
int a, b, c;                    // Basic multiple declarations
int x = 1, y, z = 3;           // Mixed initialization
int arr[5], count, total;      // Arrays with other variables
```

### Include Preprocessing
The parser supports:
```c
#include "header.h"            // Relative to current file
#include "path/to/file.h"      // Subdirectory includes
```

Features:
- **Recursive includes** with cycle detection
- **Relative path resolution**
- **Error handling** for missing files
- **Maximum depth protection** (100 files)

### Variable Scoping
The parser correctly handles:
```c
int global_var = 10;           // Global scope

int main() {
    int local_var = 20;        // Function scope
    
    {
        int block_var = 30;    // Block scope
        int local_var = 40;    // Shadows function scope
        
        {
            int nested_var = 50; // Nested block scope
            local_var = nested_var; // Uses block scope local_var
        }
        // nested_var not accessible here
    }
    // block_var not accessible here
    // local_var back to function scope (20)
}
```

Features:
- **Proper variable shadowing** - inner scope variables hide outer scope
- **Scope-aware variable lookup** - finds most recent declaration
- **Automatic scope cleanup** - variables removed when exiting scope
- **SSA form generation** - each variable declaration gets unique version

## Test Results

All tests should pass:
- ✅ Multiple variable declarations
- ✅ Multiple variables with initialization  
- ✅ Include with multiple variable declarations
- ✅ Simple logical operations
- ✅ While loops
- ✅ If-while combinations
- ✅ Function features
- ✅ For loops
- ✅ All loop constructs

## Adding New Tests

1. Create test file in `test/` directory
2. Add test case to `run_tests.sh`
3. Run `make run_tests` to verify

## Generated Files

Tests generate HDL files:
- `*_template.v` - Verilog module
- `*_tb.v` - Testbench
- `*_smdata.mem` - Microcode data
- `*_vardata.mem` - Variable data
- `Makefile.sim` - Simulation makefile
- `sim_main.cpp` - C++ simulation driver

Use `make clean` to remove generated files.
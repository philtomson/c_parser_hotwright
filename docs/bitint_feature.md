# C23 _BitInt Type Support with Bit Indexing

This C parser implements support for C23 `_BitInt` types with a unique twist: bit-level indexing capability that treats `_BitInt` variables as indexable bit arrays.

## Overview

The `_BitInt(n)` type allows you to declare integers with a specific bit width `n`, providing precise control over the number of bits used. Unlike traditional C integer types whose size depends on the platform, `_BitInt(n)` consistently uses exactly `n` bits.

## Syntax

### Declaration
```c
_BitInt(n) variable_name;
```
Where `n` is a positive integer specifying the bit width.

### Examples
```c
_BitInt(1) flag;        // Single bit (boolean-like)
_BitInt(4) nibble;      // 4-bit integer (0-15)
_BitInt(8) byte;        // 8-bit integer (0-255)
_BitInt(16) word;       // 16-bit integer
```

## Bit-Level Initialization

`_BitInt` variables can be initialized using bit patterns with brace syntax:

```c
_BitInt(3) x = {1, 0, 1};  // Sets x to binary 101 (decimal 5)
_BitInt(4) y = {1, 1, 0, 1}; // Sets y to binary 1101 (decimal 13)
```

**Bit Ordering**: Bits are specified from least significant (index 0) to most significant (index n-1).

## Bit Indexing

The unique feature of this implementation is the ability to index individual bits:

### Reading Bits
```c
_BitInt(8) value = 170;  // Binary: 10101010
if (value[0]) {          // Check bit 0 (LSB) - false
    // This won't execute
}
if (value[1]) {          // Check bit 1 - true
    // This will execute
}
```

### Writing Bits
```c
_BitInt(4) nibble = {0, 0, 0, 0};  // All bits clear
nibble[0] = 1;           // Set bit 0: nibble = 0001
nibble[3] = 1;           // Set bit 3: nibble = 1001 (decimal 9)
nibble[1] = 0;           // Clear bit 1 (already 0)
```

### Bit Indexing in Expressions
```c
_BitInt(8) flags = {1, 0, 1, 1, 0, 0, 1, 0};

// Use in conditions
if (flags[2]) {
    printf("Flag 2 is set\n");
}

// Copy bits between variables
_BitInt(4) status;
status[0] = flags[7];    // Copy MSB of flags to LSB of status
```

## Complete Example

```c
int main() {
    // Declare various _BitInt types
    _BitInt(1) flag = {1};         // Single bit flag
    _BitInt(4) nibble = {1, 1, 0, 1}; // 4-bit value = 13 (binary 1101)
    _BitInt(8) byte;               // 8-bit uninitialized
    
    // Test bit indexing in conditions
    if (flag[0]) {
        byte = 255;                // Set all bits
    }
    
    // Test bit manipulation
    if (nibble[3]) {               // Check MSB
        nibble[0] = 0;             // Clear LSB
    } else {
        nibble[3] = 1;             // Set MSB
    }
    
    // Test bit copying between variables
    byte[7] = nibble[2];           // Copy bit 2 of nibble to bit 7 of byte
    
    return 0;
}
```

## Control Flow Graph Integration

`_BitInt` types integrate seamlessly with the parser's CFG generation:

- Bit operations are represented as SSA instructions
- Bit indexing appears as array access operations in the CFG
- Conditional branches based on bit values are properly handled
- DOT file visualization shows bit manipulation in the control flow

### Example CFG Output
```
Block 0 (entry):
  t1 = 1
  flag_1 = t0
  t7 = flag_1 ? 0
  if t7 goto bb2 else bb3
```

## Implementation Details

### Parser Extensions
- Added `TOKEN_BITINT` to the lexer for `_BitInt` keyword recognition
- Extended `VarDeclNode` AST structure with `bit_width` field
- Enhanced declaration parsing to handle `_BitInt(n)` syntax
- Updated statement parsing to recognize `_BitInt` in function bodies

### AST Representation
- `_BitInt` declarations store the bit width parameter
- Bit indexing uses existing array access node types
- Initializer lists work with bit patterns

### CFG Generation
- `_BitInt` variables are treated as SSA values
- Bit operations generate appropriate SSA instructions
- Control flow analysis handles bit-based conditions

## Usage with CFG Visualization

Generate control flow graphs for `_BitInt` code:

```bash
./c_parser --dot bitint_example.c
```

This creates a DOT file showing how bit operations flow through the program, which can be visualized with:

```bash
dot -Tpng bitint_example.dot -o bitint_example.png
```

## Limitations

- Bit width `n` must be a positive integer
- Maximum bit width depends on implementation limits
- Bit indexing bounds are not checked at compile time
- No automatic bit width inference

## Future Enhancements

- Compile-time bounds checking for bit indices
- Bit manipulation optimization in CFG
- Support for bit slicing operations
- Integration with hardware description workflows
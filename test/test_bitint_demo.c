int main() {
    // Test various _BitInt widths
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
    
    // Test bit indexing in assignments
    byte[7] = nibble[2];           // Copy bit 2 of nibble to bit 7 of byte
    
    return 0;
}
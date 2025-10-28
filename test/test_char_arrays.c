int main() {
    // Char array declarations
    char str[10];
    unsigned char buffer[256];
    
    // Array element assignments
    str[0] = 72;    // 'H'
    str[1] = 105;   // 'i'
    str[2] = 0;     // null terminator
    
    buffer[0] = 255;
    buffer[1] = 128;
    buffer[2] = 0;
    
    // Array access in expressions
    char c = str[0];
    unsigned char uc = buffer[0];
    
    // Loop with char arrays
    for (int i = 0; i < 3; i = i + 1) {
        str[i] = str[i] + 1;
        buffer[i] = buffer[i] - 1;
    }
    
    return 0;
}
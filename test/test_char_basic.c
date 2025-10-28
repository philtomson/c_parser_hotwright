int main() {
    // Basic char variable declarations
    char c1 = 65;           // ASCII 'A'
    unsigned char c2 = 255; // Max unsigned char value
    char c3;                // Uninitialized signed char
    unsigned char c4;       // Uninitialized unsigned char
    
    // Assignment between char types
    c1 = c2;
    c3 = 100;
    c4 = 200;
    
    // Arithmetic with char
    c1 = c1 + 1;
    c2 = c2 - 1;
    
    return 0;
}
// Comprehensive test for char and unsigned char support

// Global char variables
char global_char = 65;          // 'A'
unsigned char global_uchar = 255;

// Function with char return type and parameters
char char_add(char a, char b) {
    return a + b;
}

// Function with unsigned char
unsigned char uchar_multiply(unsigned char x, unsigned char y) {
    if (x > 0 && y > 0 && x <= 255/y) {
        return x * y;
    }
    return 255;
}

// Mixed parameter types
int process_chars(char c, unsigned char uc, int i) {
    return c + uc + i;
}

int main() {
    // Local char variables
    char local_char = 72;       // 'H'
    unsigned char local_uchar = 200;
    
    // Char arrays
    char message[5];
    unsigned char buffer[10];
    
    // Array assignments
    message[0] = 72;  // 'H'
    message[1] = 101; // 'e'
    message[2] = 108; // 'l'
    message[3] = 108; // 'l'
    message[4] = 111; // 'o'
    
    buffer[0] = 255;
    buffer[1] = 128;
    buffer[2] = 64;
    
    // Function calls
    char result1 = char_add(local_char, 5);
    unsigned char result2 = uchar_multiply(10, 20);
    int result3 = process_chars(local_char, local_uchar, 100);
    
    // Arithmetic operations
    local_char = local_char + 1;
    local_uchar = local_uchar - 50;
    
    // Assignments between types
    local_char = global_char;
    local_uchar = global_uchar;
    
    // Control flow with char
    if (local_char > 65) {
        local_char = 65;
    }
    
    // Loop with char
    char i;
    for (i = 0; i < 5; i = i + 1) {
        message[i] = message[i] + 1;
    }
    
    return 0;
}
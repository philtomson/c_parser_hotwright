char process_char(char input) {
    return input + 1;
}

unsigned char process_unsigned(unsigned char input) {
    if (input < 255) {
        return input * 2;
    }
    return 255;
}

char get_ascii_value() {
    return 65; // 'A'
}

unsigned char get_max_unsigned() {
    return 255;
}

int main() {
    char c = 65;        // 'A'
    unsigned char uc = 200;
    
    // Function calls with char parameters
    c = process_char(c);
    uc = process_unsigned(uc);
    
    // Function calls returning char
    char ascii = get_ascii_value();
    unsigned char max_val = get_max_unsigned();
    
    // Mixed operations
    c = process_char(ascii);
    uc = process_unsigned(max_val);
    
    return 0;
}
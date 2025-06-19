int main() {
    // Variable declarations with initialization
    int a = 5;
    int b = 10;
    int c = 15;
    
    // Simple assignments
    a = 20;
    b = a;
    
    // Arithmetic expressions
    c = a + b;
    c = a - b;
    c = a * b;
    c = a / b;
    
    // Parenthesized expressions
    c = (a + b) * 2;
    c = a + (b * 2);
    
    // Comparison expressions
    c = a < b;
    c = a > b;
    c = a <= b;
    c = a >= b;
    c = a == b;
    c = a != b;
    
    // Switch statement with cases and default
    switch (a) {
        case 5:
            b = 100;
            break;
        case 10:
            b = 200;
            break;
        case 20:
            b = 300;
            break;
        default:
            b = 0;
    }
}
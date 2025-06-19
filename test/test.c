int main() {
    int a = 10;
    int b = 20;
    int result = 0;
    
    // Test arithmetic operations
    result = a + b;
    result = a * 2;
    result = b - a;
    result = b / 2;
    
    // Test switch statement
    switch (a) {
        case 5:
            result = 100;
            break;
        case 10:
            result = 200;
            break;
        case 15:
            result = 300;
            break;
        default:
            result = 0;
    }
    
    // Test nested arithmetic
    result = (a + b) * 2;
    result = a + (b * 3);
    
    // Test comparisons
    if (a < b) {
        result = 1;
    }
    
    if (a == 10) {
        result = a * a;
    }
    
    // Test while loop
    while (a > 0) {
        result = result + 1;
        a = a - 1;
    }
    
    return result;
}
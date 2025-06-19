int main() {
    int a = 10;
    int b = 20;
    int result = 0;
    
    // Simple if statement
    if (a < b) {
        result = 1;
    }
    
    // If-else statement
    if (a > b) {
        result = 2;
    } else {
        result = 3;
    }
    
    // Nested if statements
    if (a < b) {
        if (a == 10) {
            result = 4;
        }
    }
    
    // If-else if-else chain
    if (a > b) {
        result = 5;
    } else if (a == b) {
        result = 6;
    } else {
        result = 7;
    }
    
    // If with complex expressions
    if ((a + 5) < (b - 5)) {
        result = 8;
    }
    
    // If without braces (single statement)
    if (a != b)
        result = 9;
    
    // If-else without braces
    if (a == b)
        result = 10;
    else
        result = 11;
}
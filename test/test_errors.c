int main() {
    _BitInt(0) x;          // Error: bit width must be positive
    _BitInt(4) y = {1, 0, 1, 1};
    
    if (y[2] {             // Error: missing closing parenthesis
        y[0] = 1;
    }
    
    return 0;
}
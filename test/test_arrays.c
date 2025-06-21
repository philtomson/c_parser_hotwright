// Test array declarations and indexing
int main() {
    // Integer array
    int numbers[10];
    numbers[0] = 5;
    numbers[1] = 10;
    int x = numbers[0] + numbers[1];
    
    // Boolean array
    bool flags[5];
    flags[0] = true;
    flags[1] = false;
    flags[2] = flags[0];
    
    // Array access in expressions
    int i = 3;
    numbers[i] = 20;
    int y = numbers[i] * 2;
    
    // Array access in conditions
    if (flags[0]) {
        x = 100;
    }
    
    // Array access in loops
    for (int j = 0; j < 5; j = j + 1) {
        numbers[j] = j * j;
    }
    
    return x;
}
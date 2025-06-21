// Test array initializer lists
bool signals[4] = { 1, 0, 1, 0 };
int numbers[3] = { 10, 20, 30 };

int main() {
    // Test accessing initialized array elements
    if (signals[0]) {
        numbers[0] = 100;
    }
    
    // Test loop with initialized arrays
    for (int i = 0; i < 3; i = i + 1) {
        numbers[i] = numbers[i] + signals[i];
    }
    
    return numbers[2];
}
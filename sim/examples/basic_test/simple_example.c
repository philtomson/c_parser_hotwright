// Simple C example for testing the hotstate simulator
// This will be used to generate the memory files

int main() {
    int a = 0;
    int b = 0;
    int result = 0;
    
    // Simple conditional logic that will generate interesting state transitions
    if (a == 0) {
        result = 1;
    } else {
        result = 2;
    }
    
    // Simple loop structure
    for (int i = 0; i < 3; i++) {
        if (i == 1) {
            result = result + 10;
        } else {
            result = result + 1;
        }
    }
    
    // Switch statement
    switch (b) {
        case 0:
            result = result + 100;
            break;
        case 1:
            result = result + 200;
            break;
        default:
            result = result + 300;
            break;
    }
    
    return result;
}
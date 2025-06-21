// Test hardware variable scoping compatibility
// This tests whether our new scoping system breaks hardware variable detection

int main() {
    // Hardware state variables (LED pattern)
    int LED0 = 1;   // Should be detected as state variable
    int LED1 = 0;   // Should be detected as state variable
    
    // Hardware input variables
    int a0;  // Should be detected as input variable
    int a1;  // Should be detected as input variable
    
    // Test scoping with hardware variables
    if (a0 > 0) {
        int LED2 = 1;  // State variable in nested scope
        LED0 = LED2 && a1; // Use both state and input variables
    }
    
    // Test variable shadowing with hardware names
    {
        int a0 = 5;  // This shadows the input variable a0
        if (a0 > 3) {
            LED1 = 1;
        } else {
            LED1 = 0;
        }
    }
    
    return LED0;
}
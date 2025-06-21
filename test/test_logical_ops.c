int main() {
    // State variables (outputs)
    int LED0 = 0;
    int LED1 = 0;  
    int LED2 = 1;
    
    // Input variables
    int a0;
    int a1; 
    int a2;
    
    // Test logical AND operator
    if (a0 == 0 && a1 == 1) {
        LED0 = 1;
        LED0 = 0;
    }
    
    // Test logical OR operator
    if (a1 == 0 || a2 == 1) {
        LED1 = 1;
    }
    
    // Test combined logical operators
    if ((a0 == 1 && a2 == 0) || a1 == 1) {
        LED2 = 1;
    }
    
    return 0;
}
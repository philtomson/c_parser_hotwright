int main() {
    // State variables (outputs) - using global-style declarations
    int LED0 = 0; /* state0 */
    int LED1 = 0; /* state1 */
    int LED2 = 1; /* state2 */
    
    // Input variables
    int a0;
    int a1;
    int a2;
    
    // Main logic with logical operators (similar to hotstate example)
    while(1) {
        if(a0 == 0 && a1 == 1) {
            LED0 = 1;
            LED0 = 0; // LED0 will equal 0
        }
        
        if(a1 == 0 || a2 == 1) {
            if(a0 == 0) {
                LED1 = 1;
            }
        }
        
        if(a0 == 1 && a2 == 0) {
            LED2 = 1;
        }
        
        if(a0 == 0 && a2 == 0) {
            LED0 = 0;
            LED1 = 0;
            LED2 = 0;
        }
        
        if(a0 == 0) {
            LED0 = 1;
        }
    }
    
    return 0;
}
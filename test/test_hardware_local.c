int main() {
    // State variables (outputs) - local boolean variables with initialization
    bool LED0 = 0;
    bool LED1 = 0;  
    bool LED2 = 1;
    
    // Input variables - local boolean variables without initialization
    bool a0;
    bool a1; 
    bool a2;
    
    // Simple hardware logic
    if (a0) {
        LED0 = 1;
    }
    
    if (a1) {
        LED1 = 1;
    }
    
    return 0;
}
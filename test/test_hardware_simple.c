// Test file for hardware analysis - hotstate compatible format
#include <stdbool.h>

// State variables (outputs) - following hotstate pattern
bool LED0 = 0;  // state0
bool LED1 = 0;  // state1  
bool LED2 = 1;  // state2

// Input variables - following hotstate pattern
bool a0, a1, a2;

int main() {
    while(1) {
        if(a0 == 0 && a1 == 1)
            LED0 = 1;
        else if(a1 == 0 || a2 == 1)
            LED1 = 1;
        
        if(a0 == 1 && a2 == 0)
            LED2 = 1;
            
        if(a0 == 0 && a2 == 0)
            LED0 = 0, LED1 = 0, LED2 = 0;
    }
    return 0;
}
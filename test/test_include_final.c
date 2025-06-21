#include "simple_header.h"

int main() {
    int a0 = 0;
    int a1 = 1; 
    int a2 = 0;
    int LED0;
    int LED1;
    int LED2;
    
    // Test logical operations with included header
    if (a0 == 0 && a1 == 1) {
        LED0 = 1;
        LED1 = 0;
        LED2 = 0;
    } else {
        LED0 = 0;
        LED1 = 1;
        LED2 = 1;
    }
    return 0;
}
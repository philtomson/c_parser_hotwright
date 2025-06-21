#include "test_header.h"

int main() {
    // Test logical operations with included variables
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

int test_function(int x) {
    return x * 2;
}
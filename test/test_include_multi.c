#include "test_header_multi.h"

int main() {
    // Test logical operations with included multi-variable declarations
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
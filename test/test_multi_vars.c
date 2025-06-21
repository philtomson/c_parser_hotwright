int main() {
    int a0, a1, a2;
    int LED0, LED1, LED2;
    
    a0 = 0;
    a1 = 1;
    a2 = 0;
    
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
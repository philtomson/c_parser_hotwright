int main() {
    int sum = 0;
    
    for (int i = 0; i < 10; i = i + 1) {
        sum = sum + i;
    }
    
    for (int j = 10; j > 0; j = j - 1) {
        sum = sum + j;
    }
    
    int k = 0;
    for (; k < 5; k = k + 1) {
        sum = sum + 1;
    }
    
    for (int m = 0; ; m = m + 1) {
        if (m >= 5) {
            break;
        }
        sum = sum + m;
    }
    
    for (;;) {
        if (sum > 100) {
            break;
        }
        sum = sum + 1;
    }
    
    for (int x = 0; x < 3; x = x + 1) {
        for (int y = 0; y < 3; y = y + 1) {
            sum = sum + x * y;
        }
    }
    
    return 0;
}
int main() {
    int sum = 0;
    int i = 0;
    
    while (i < 5) {
        sum = sum + i;
        i = i + 1;
    }
    
    for (int j = 0; j < 5; j = j + 1) {
        sum = sum + j;
    }
    
    for (int x = 0; x < 3; x = x + 1) {
        int y = 0;
        while (y < 3) {
            for (int z = 0; z < 2; z = z + 1) {
                sum = sum + x + y + z;
            }
            y = y + 1;
        }
    }
    
    i = 10;
    while (i > 0) {
        if (i == 5) {
            break;
        }
        i = i - 1;
    }
    
    for (;;) {
        if (sum > 100) {
            break;
        }
        sum = sum + 1;
    }
    
    return 0;
}
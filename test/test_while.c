int main() {
    int i = 0;
    int sum = 0;
    
    while (i < 10) {
        sum = sum + i;
        i = i + 1;
    }
    
    while (i > 0) {
        i = i - 1;
    }
    
    int j = 5;
    while (j) {
        j = j - 1;
    }
    
    int x = 1;
    int y = 10;
    while (x < y) {
        x = x * 2;
    }
    
    while (x > 100) {
        if (x > 200) {
            x = x - 50;
        } else {
            x = x - 10;
        }
    }
}
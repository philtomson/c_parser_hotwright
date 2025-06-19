int main() {
    int x = 10;
    int y = 20;
    int result = 0;
    
    if (x < y) {
        result = 1;
    }
    
    if (x > y) {
        result = 2;
    } else {
        result = 3;
    }
    
    if (x == 10) {
        if (y == 20) {
            result = 4;
        }
    }
    
    if (x > 100) {
        result = 5;
    } else if (x > 50) {
        result = 6;
    } else if (x > 0) {
        result = 7;
    } else {
        result = 8;
    }
}
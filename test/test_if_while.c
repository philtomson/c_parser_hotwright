int main() {
    int count = 0;
    int max = 10;
    
    while (count < max) {
        if (count == 5) {
            count = count + 2;
        } else {
            count = count + 1;
        }
    }
    
    int x = 100;
    while (x > 0) {
        if (x > 50) {
            x = x - 10;
        } else if (x > 25) {
            x = x - 5;
        } else {
            x = x - 1;
        }
    }
    
    int i = 0;
    int j = 0;
    while (i < 5) {
        j = 0;
        while (j < 3) {
            if (i == j) {
                j = j + 2;
            } else {
                j = j + 1;
            }
        }
        i = i + 1;
    }
}
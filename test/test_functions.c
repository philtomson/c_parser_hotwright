int add(int a, int b) {
    return a + b;
}

int multiply(int x, int y) {
    return x * y;
}

int factorial(int n) {
    if (n <= 1) {
        return 1;
    } else {
        return n * factorial(n - 1);
    }
}

int max(int a, int b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}

int sum_range(int start, int end) {
    int sum = 0;
    int i = start;
    while (i <= end) {
        sum = sum + i;
        i = i + 1;
    }
    return sum;
}

int main() {
    int x = 5;
    int y = 10;
    int result = 0;
    
    result = add(x, y);
    result = multiply(3, 4);
    result = factorial(5);
    result = max(x, y);
    result = sum_range(1, 10);
    
    return 0;
}
// Comprehensive test for function call handling
// Tests various scenarios to ensure function calls are parsed and processed correctly

// Simple functions for testing
int add(int a, int b) {
    return a + b;
}

int multiply(int x, int y) {
    return x * y;
}

int get_value() {
    return 42;
}

int max(int a, int b, int c) {
    int temp = a;
    if (b > temp) temp = b;
    if (c > temp) temp = c;
    return temp;
}

int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

int main() {
    int x = 10;
    int y = 20;
    int z = 30;
    int result = 0;

    // Basic function calls with variables
    result = add(x, y);
    result = multiply(x, z);

    // Function calls with literals
    result = add(5, 10);
    result = multiply(3, 7);

    // Function call with no arguments
    result = get_value();

    // Function call with multiple arguments
    result = max(x, y, z);

    // Nested function calls
    result = add(multiply(x, y), multiply(y, z));
    result = multiply(add(x, 5), add(y, 10));

    // Function calls in complex expressions
    result = add(x, y) * multiply(z, 2) + get_value();

    // Function calls with expressions as arguments
    result = add(x + y, z * 2);
    result = multiply(x * 2, y + z);

    // Multiple function calls in one expression
    result = add(add(x, y), add(z, get_value()));

    // Function calls in conditional expressions
    if (add(x, y) > multiply(x, z)) {
        result = max(x, y, z);
    } else {
        result = get_value();
    }

    // Function calls in loops
    int sum = 0;
    int i = 1;
    while (i <= 5) {
        sum = add(sum, multiply(i, 2));
        i = add(i, 1);
    }

    // Function calls in array operations (if supported)
    // Note: This tests function calls in more complex contexts

    return result;
}
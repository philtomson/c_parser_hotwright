// Test case to verify if-else statements inside functions are properly handled
// This tests whether control flow inside functions is captured in microcode

int conditional_function(int x, int y) {
    int result;
    if (x > y) {
        result = x + y;
    } else {
        result = x * y;
    }
    return result;
}

int nested_if_function(int a, int b, int c) {
    int temp;
    if (a > 0) {
        if (b > c) {
            temp = a + b;
        } else {
            temp = a + c;
        }
    } else {
        if (b < c) {
            temp = b - c;
        } else {
            temp = c - b;
        }
    }
    return temp;
}

int main() {
    int x = 10;
    int y = 5;
    int z = 15;

    // Call function with if-else inside
    int result1 = conditional_function(x, y);

    // Call function with nested if-else inside
    int result2 = nested_if_function(x, y, z);

    // Use results in main
    if (result1 > result2) {
        return 0;
    } else {
        return 1;
    }
}
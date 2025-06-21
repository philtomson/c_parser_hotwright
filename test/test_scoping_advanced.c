int global_x = 100;

int main() {
    int x = 1;              // Function scope x
    int y = 2;
    
    {
        int x = 10;         // Block scope x (shadows function x)
        int z = 20;
        
        {
            int x = 30;     // Nested block x (shadows block x)
            int w = x + z;  // Should use nested x (30) and block z (20)
            y = w;          // Should modify function scope y
        }
        
        // x should be back to block scope (10)
        // z should still be accessible (20)
        // y should be modified (50)
        global_x = x + z + y;  // 10 + 20 + 50 = 80
    }
    
    // x should be back to function scope (1)
    // y should be modified (50)
    // z should not be accessible here
    return x + y + global_x;  // 1 + 50 + 80 = 131
}
int main() {
    _BitInt(3) x = {1, 0, 1};  // x is set to 5 (binary 101)
    
    if (x[1]) {                // test bit indexing in if condition
        x[2] = 0;              // clear the most significant bit
    } else {
        x[0] = 0;              // clear the least significant bit
    }
    
    return 0;
}
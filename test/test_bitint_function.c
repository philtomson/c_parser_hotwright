int main() {
    _BitInt(3) x = {1, 0, 1};  // x is set to 5 (binary 101)
    _BitInt(4) y = 12;         // y is set to 12 (binary 1100)
    _BitInt(8) z;              // uninitialized 8-bit integer
    
    x[1] = 1;                  // now x is 7 (binary 111)
    y[0] = 0;                  // now y is 12 (binary 1100, bit 0 was already 0)
    z = 255;                   // z is set to 255 (binary 11111111)
    
    return 0;
}
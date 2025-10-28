int main() {
    int x = 0;
    
    goto skip;
    x = 1;  // This should be skipped
    
skip:
    x = 2;
    
    if (x == 2) {
        goto end;
    }
    
    x = 3;  // This should be skipped
    
end:
    return x;
}
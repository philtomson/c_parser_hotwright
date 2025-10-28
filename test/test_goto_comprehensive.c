int test_goto_patterns() {
    int result = 0;
    
    // Test 1: Forward jump
    goto skip_increment;
    result = result + 100;  // Should be skipped
    
skip_increment:
    result = result + 1;
    
    // Test 2: Backward jump (simple loop)
    int counter = 0;
    
loop_start:
    counter = counter + 1;
    result = result + counter;
    
    if (counter < 3) {
        goto loop_start;
    }
    
    // Test 3: Conditional goto
    if (result > 5) {
        goto success;
    }
    
    result = -1;  // Should be skipped
    goto end;
    
success:
    result = result * 2;
    
end:
    return result;
}
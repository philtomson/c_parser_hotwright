// This is a single-line comment
int main() {
    // Variable declarations with comments
    int a = 5;  // Initialize a to 5
    int b = 10; // Initialize b to 10
    
    /* This is a multi-line comment
       that spans multiple lines
       and contains various information */
    int c = a + b;
    
    /* Single line multi-line comment */
    c = c * 2;
    
    // Nested comment test
    /* This is a comment with // inside it */
    
    // Function calls
    /* Comment before statement */ a = b; /* Comment after statement */
    
    // Complex expressions with comments
    c = a + /* comment in middle */ b;
    
    // End of function comment
    return 0; // Return success
} // End of main function

/* Final comment at end of file */
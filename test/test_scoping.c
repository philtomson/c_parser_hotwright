int global_var = 10;

int main() {
    int local_var = 20;
    
    {
        int block_var = 30;
        int local_var = 40;  // Should shadow the outer local_var
        
        if (block_var > 0) {
            int nested_var = 50;
            local_var = nested_var;  // Should refer to the inner local_var
        }
        
        global_var = local_var;  // Should use inner local_var (40->50)
    }
    
    // local_var should be back to 20 here (outer scope)
    return local_var + global_var;
}
int main() {
    int x = 3;
    int result = 0;
    
    switch (x) {
        case 1:
        case 2:
            result = 10;
            break;
        case 3:
            result = 20;
            // fall through
        case 4:
            result = result + 5;
            break;
        default:
            result = 0;
    }
    
    return result;
}
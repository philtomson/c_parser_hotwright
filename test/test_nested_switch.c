int main() {
    int a = 5;
    int b = 2;
    int result = 0;
    
    switch (a) {
        case 5:
            switch (b) {
                case 1:
                    result = 10;
                    break;
                case 2:
                    result = 20;
                    break;
                default:
                    result = 5;
            }
            break;
        case 10:
            switch (b) {
                case 1:
                    result = 100;
                    break;
                case 2:
                    result = 200;
                    break;
                default:
                    result = 50;
            }
            break;
        default:
            result = 0;
    }
    
    return result;
}
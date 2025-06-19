int main() {
    int a = 5;
    int b = 10;
    int c = 15;
    
    a = 20;
    b = a;
    
    c = a + b;
    c = a - b;
    c = a * b;
    c = a / b;
    
    c = (a + b) * 2;
    c = a + (b * 2);
    
    c = a < b;
    c = a > b;
    c = a <= b;
    c = a >= b;
    c = a == b;
    c = a != b;
    
    if (a < b) {
        c = 1;
    }
    
    if (a > b) {
        c = 2;
    } else {
        c = 3;
    }
    
    if (a < b) {
        if (a == 5) {
            c = 4;
        } else {
            c = 5;
        }
    }
    
    if (a > b) {
        c = 6;
    } else if (a == b) {
        c = 7;
    } else {
        c = 8;
    }
    
    while (a > 0) {
        a = a - 1;
        if (a == 5) {
            break;
        }
    }
    
    int i = 0;
    while (i < 10) {
        c = c + i;
        i = i + 1;
    }
    
    switch (a) {
        case 5:
            b = 100;
            break;
        case 10:
            b = 200;
            break;
        case 20:
            b = 300;
            break;
        default:
            b = 0;
    }
}
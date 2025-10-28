int b = 0;

void test_switch(int a) {
   int b;
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
    //return  b;
}


int main() {
    int a = 5;
    a   = test_switch(a);
    /*
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
    } */
}

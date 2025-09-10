int case_in;
int new_case;
int state0 = 0;
int state1 = 0;
int state2 = 0;
int main() {


    while (1) {
        switch (case_in) {
            case 0:
                state1 = 0; state2 = 0;
                continue;
            case 1:
                state1 = 0; state2 = 1;
                break;
            case 2:
                state1 = 1; state2 = 0;
                break;
            case 3:
                state1 = 1; state2 = 1;
                continue;
        }
        switch (new_case) {
            case 0:
                state0 = 1;
                continue;
            case 1:
                state2 = 0;
                continue;
            default:
                state1 = 0;
                break;
        }
    }
}

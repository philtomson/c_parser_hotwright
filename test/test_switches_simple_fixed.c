int state0 = 0;
int state1 = 0;
int state2 = 0;

int case_in = 0;
int new_case = 0;

int main() {
    switch (case_in) {
        case 0:
            state1 = 0;
            state2 = 0;
            break;
        case 1:
            state1 = 0;
            state2 = 1;
            break;
        case 2:
            state1 = 1;
            state2 = 0;
            break;
        case 3:
            state1 = 1;
            state2 = 1;
            break;
        case 4:
            switch (new_case) {
                case 0:
                    state0 = 1;
                    break;
                case 1:
                    state2 = 0;
                    break;
                default:
                    state1 = 0;
                    break;
            }
            break;
        default:
            state1 = 0;
            state2 = 0;
            state0 = 0;
    }

    switch (new_case) {
        case 0:
            state0 = 1;
            state2 = 0;
            break;
        case 1:
            state0 = 0;
            state2 = 0;
            break;
        case 2:
            state0 = 0;
            state2 = 1;
            break;
        case 3:
            state0 = 1;
            state2 = 1;
            break;
        default:
            state0 = 0;
            break;
    }
    
    return 0;
}
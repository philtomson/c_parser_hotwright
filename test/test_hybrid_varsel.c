/* Test hybrid varSel assignment */

int state0 = 0; /* Test output 0 */
int state1 = 0; /* Test output 1 */
int state2 = 0; /* Test output 2 */

/* Input variables */
int a0, a1, a2;

void main() {
    /* Test case 1: Only simple variable conditionals (should use varSel = 0) */
    while(1) {
        if (a0) {
            state0 = 1;  // Simple variable - varSel = 0
        }

        if (a1) {
            state1 = 1;  // Simple variable - varSel = 0
        } else {
            state1 = 0;  // Simple variable - varSel = 0
        }

        /* Test case 2: Mix of simple and complex conditionals */
        if (a0) {
            state2 = 1;  // Simple variable - varSel = 0
        }

        if (a0 && a1) {
            state0 = 0;  // Complex expression - varSel = 1
        }

        if (a2) {
            state1 = 1;  // Simple variable - varSel = 0
        }

        /* Test case 3: Only complex conditionals */
        if (a0 && a1) {
            state2 = 1;  // Complex expression - varSel = 2
        }

        if (a1 || a2) {
            state0 = 1;  // Complex expression - varSel = 3
        }

        /* Test case 4: No conditionals at all - varSel should be 'X' */
        state1 = a0;  // Assignment only, no conditional
        state2 = a1;  // Assignment only, no conditional
    }
}
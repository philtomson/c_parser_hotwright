
bool state0 = 0;
bool state1 = 0;
bool state2 = 0;

bool fifo_full, fifo_empty;

void 
main() {

while (fifo_empty) {
    state0 = 1;
    while(!fifo_empty) {
       state0 = 0, state1 = 1;
    }
 }
}

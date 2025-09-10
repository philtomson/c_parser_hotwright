
bool state[3] = {0,0,0};
/*
bool state0 = 0;
bool state1 = 0;
bool state2 = 0;
*/

bool case_in[7], new_case[3];


void main() {

   while(1) {
      switch (case_in) {
       case 0:  {state1 = 0, state2 = 0;  continue;}
       case 1:  {state1 = 0, state2 = 1;  break;}
       case 2:  {state1 = 1, state2 = 0;  break;}
       case 3:  {state1 = 1, state2 = 1;  continue;}
       case 4:  {switch (new_case) {
                    case 0: {state0 = 1;  continue;}
                    case 1: {state2 = 0;  continue;}
                    default:{state1 = 0;  break;}
                  }
                 break;
                }
       default: { continue;}
      }
      switch (new_case) {
       case 0:  {state0 = 1, state2 = 0;  break;}
       case 1:  {state0 = 0, state2 = 0;  continue;}
       case 2:  {state0 = 0, state2 = 1;  break;}
       case 3:  {state0 = 1, state2 = 1;  continue;}
       default: {state0 = 0;  break;}
      }
   }
}


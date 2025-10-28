/* was:
bool nst0 = 0;
bool nst1 = 0;
bool nst2 = 0;
*/

bool nst[3] = {0,0,0};


bool case_in[7], new_case[3];

void main() {

   while(1) {
      switch (case_in) {
       case 0:  {nst[1] = 0, nst[2] = 0;  continue;}
       case 1:  {nst[1] = 0, nst[2] = 1;  break;}
       case 2:  {nst[1] = 1, nst[2] = 0;  break;}
       case 3:  {nst[1] = 1, nst[2] = 1;  continue;}
       case 4:  {switch (new_case) {
                    case 0: {nst[0] = 1;  continue;}
                    case 1: {nst[2] = 0;  continue;}
                    default:{nst[1] = 0;  break;}
                  }
                 break;
                }
       default: { nst[1]=0; nst[2]=0; nst[0]=0; }
      }

      switch (new_case) {
       case 0:  {nst[0] = 1, nst[2] = 0;  break;}
       case 1:  {nst[0] = 0, nst[2] = 0;  continue;}
       case 2:  {nst[0] = 0, nst[2] = 1;  break;}
       case 3:  {nst[0] = 1, nst[2] = 1;  continue;}
       default: {nst[0] = 0;  break;}
      }
   }
}


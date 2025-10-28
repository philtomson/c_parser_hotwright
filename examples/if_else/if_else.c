/* Steve Casselman */

bool LED0 = 0; /* state0 */
bool LED1 = 0; /* state1 */
bool LED2 = 1; /* state2 */


/* inputs */
bool  a0,a1,a2;


void
main()
{

/* main loop */
 while(1)
 {
  if(a0 == 0 && a1 == 1)
    LED0 = 1;
  else if((a1 == 0 || a2 == 1 ) & !a0)
    LED1 = 1;
  if(a0 == 1 && a2 == 0){
    LED2 = 1;
    if (a0) LED1 = 0;
  }
  if(a0 == 0 && a2 == 0 & !a2)
    LED0 = 0, LED1 = 0, LED2 = 0;
  if (!a0) LED0 = 1;
 } /* end while */
}

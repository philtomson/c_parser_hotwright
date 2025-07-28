/* Steve Casselman */

bool led0 = 0; /* state0 */
bool led1 = 0; /* state1 */
bool led2 = 1; /* state2 */


/* inputs */
bool  a0,a1,a2;


void
main()
{
/* main loop */
 while(1)
 {
  if(a0 == 0 && a1 == 1)
    led0 = 1, led0 = 0;//led0 will equal 0
  else if((a1 == 0 || a2 == 1 ) & !a0)
    led1 = 1;
  if(a0 == 1 && a2 == 0)
    led2 = 1;
  if(a0 == 0 && a2 == 0 & !a2)
    led0 = 0, led1 = 0, led2 = 0;
  if (!a0) led0 = 1;
 } /* end while */
}

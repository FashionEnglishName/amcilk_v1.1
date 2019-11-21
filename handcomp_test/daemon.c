#include <stdio.h>

#include "nqueens.h"
#include "fib.h"
#include "mm_dac.h"
#include "cilksort.h"
#include "bing_h.h"


extern int nqueens_cilk_main(int n);
extern int fib_cilk_main(int n);
extern int mm_dac_cilk_main(int n);
extern int cilksort_cilk_main(int n);
extern int bing_h_cilk_main(int n);

int cilk_main(int program_id, int input) {
    int res = 0;
    switch (program_id) {
        case 0: 
            //printf("%d, to run nqueens\n", program_id);
            res = nqueens_cilk_main(input); 
            break;
        case 1: 
            //printf("%d, to run fib\n", program_id);
            res = fib_cilk_main(input); 
            break;
        case 2: 
            //printf("%d, to run mm_dac\n", program_id);
            res = mm_dac_cilk_main(input); 
            break;
        case 3: 
            //printf("%d, to run cilksort\n", program_id);
            res = cilksort_cilk_main(input); 
            break;
        case 4:
            res = bing_h_cilk_main(input); 
            break;

        default:
            break;
    }
    return res;
}
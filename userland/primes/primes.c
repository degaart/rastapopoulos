#include "runtime.h"
#include "debug.h"

static unsigned is_prime(unsigned num)
{
    if(num == 1)
        return 1;

    for(unsigned i = 2; i < num; i++) {
        if(!(num % i))
            return 0;
    }

    return 1;
}

void main()
{
    unsigned start_val = 5001000;

    /* Fork into other tasks */
    int pid = fork();
    if(!pid) {
        start_val = 5002000;
    } else {
        pid = fork();
        if(!pid) {
            start_val = 5003000;
        }
    }

    unsigned end_val = start_val + 500;
    for(unsigned i = start_val; i < end_val; i++) {
        if(is_prime(i)) {
            trace("%d", i);
        }
    }
    trace("Done");
}







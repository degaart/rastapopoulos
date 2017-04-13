#include "runtime.h"
#include "debug.h"

static unsigned fibonacci(unsigned n)
{
    unsigned result;

    if (n == 0)
        result = 0;
    else if (n == 1)
        result = 1;
    else
        result = fibonacci(n - 1) + fibonacci(n - 2);

    return result;
} 


void main()
{
    for(unsigned i = 0; i < 37; i++) {
        unsigned fib = fibonacci(i);
        trace("fib(%d): %d", i, fib);
    }

    trace("Done");
}


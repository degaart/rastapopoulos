#include "runtime.h"
#include "debug.h"

void main()
{
    setname("sleeper");

    for(unsigned i = 0; i < 20; i++) {
        sleep(1000);
        trace("sleeper: %d", i);
    }

    trace("sleeper: done");
}






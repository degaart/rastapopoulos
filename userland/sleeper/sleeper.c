#include "runtime.h"
#include "debug.h"

void main()
{
    for(unsigned i = 0; i < 20; i++) {
        sleep(1000);
        trace("%d", i);
    }

    trace("Done");
}






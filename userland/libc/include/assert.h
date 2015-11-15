#ifndef _ASSERT_H_
#define _ASSERT_H_

#include <rasta.h>
#include <stdlib.h>

#define assert(cond) \
    while(!(cond)) { \
        rs_trace("Assertion failed:\n\t%s", #cond); \
        exit(1); \
    }

#endif //_ASSERT_H_


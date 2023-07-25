#include "debug.h"
#include <string.h>

struct test_t {
    char name[32];
    testfn_t fn;
};

#define MAX_TESTS 16
static struct test_t tests[MAX_TESTS];

void add_test(const char* name, testfn_t testfn)
{
    for(size_t i = 0; i < MAX_TESTS; i++) {
        if(!tests[i].fn) {
            strlcpy(tests[i].name, name, sizeof(tests[i].name));
            tests[i].fn = testfn;
            return;
        }
    }
    PANIC("MAX_TESTS exceeded");
}

void run_tests()
{
    for(size_t i = 0; i < MAX_TESTS; i++) {
        if(tests[i].fn) {
            TRACE("Running test `%s`", tests[i].name);
            tests[i].fn();
        }
    }
}

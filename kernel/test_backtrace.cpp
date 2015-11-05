#include <stdint.h>
#include "backtrace.h"

extern "C" void fn4() {
    backtrace();
}

extern "C" void fn3() {
    fn4();
}

extern "C" void fn2() {
    fn3();
}

extern "C" void fn1() {
    fn2();
}

extern "C" void fn0() {
    fn1();
}

void test_backtrace(multiboot_info_t* multiboot_info) {
    fn0();
}

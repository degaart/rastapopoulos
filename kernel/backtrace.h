#ifndef _BACKTRACE_H_
#define _BACKTRACE_H_

#include "multiboot.h"

extern "C" void backtrace();
extern "C" void load_symbols(multiboot_info_t* multiboot_info);


#endif //_BACKTRACE_H_

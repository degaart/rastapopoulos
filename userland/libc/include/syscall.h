#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <stdint.h>
#include "../../../kernel/syscall_nums.h"

uint32_t rs_syscall(uint32_t function, uint32_t param0, uint32_t param1, uint32_t param2);

#endif //_SYSCALL_H_


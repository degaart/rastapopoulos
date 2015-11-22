#ifndef _KERNEL_TASK_H_
#define _KERNEL_TASK_H_

#include <stdint.h>

class KernelTask {
private:
    static void child1();
    static void child2();
public:
    static void entry();
};

#endif //_KERNEL_TASK_H_

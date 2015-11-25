#ifndef _KERNEL_MSG_H_
#define _KERNEL_MSG_H_

// KernelTask messages

#define MSG_TRACE                           0x100

#define MSG_EXIT                            0x200
// #define MSG_SLEEP                           0x201
#define MSG_EXEC                            0x202

#define MSG_MMAP                            0x300

#define MSG_OUTB                            0x400
#define MSG_INB                             0x401
#define MSG_OUTW                            0x402
#define MSG_INW                             0x403
#define MSG_OUTD                            0x404
#define MSG_IND                             0x405

#define MSG_GETTICKS                        0x500


#endif //_KERNEL_MSG_H_



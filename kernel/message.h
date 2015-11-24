#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include <stdint.h>

class Process;

struct Message_t {  
    void*       payload;
    Process*    sender;
    uint32_t    id;                 /* Action to perform */
    uint32_t    result_port;        /* Port to write results to */
    uint32_t    result;             /* Result of action */
    uint32_t    payload_size;       /* Payload size */
};

#endif //_MESSAGE_H_

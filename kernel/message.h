#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include <stdint.h>

struct Message_t {  
    uint32_t    id;                 /* Action to perform */
    uint32_t    result_port;        /* Port to write results to */
    uint32_t    result;             /* Result of action */
    uint8_t     payload[32];        /* Application-defined */
};

#endif //_MESSAGE_H_

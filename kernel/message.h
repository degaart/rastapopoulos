#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include <stdint.h>

#ifdef __cplusplus
class Process;
#else
typedef struct Process Process;
#endif

struct Message_t {  
    void*       payload;
    Process*    sender;
    uint32_t    id;                 /* Action to perform */
    uint32_t    result_port;        /* Port to write results to */
    uint32_t    result;             /* Result of action */
    uint32_t    payload_size;       /* Payload size */
    uint32_t    payload_checksum;   /* Checksum for payload */
};

void msg_update_checksum(Message_t* msg);
void msg_check_checksum(const Message_t* msg);
void msg_copy(Message_t* dst, const Message_t* msg);

#endif //_MESSAGE_H_

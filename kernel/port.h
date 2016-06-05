#ifndef _PORT_H_
#define _PORT_H_

#include <stdint.h>
#include "linked_list.h"
#include "message.h"

class Process;

class Port {
public:
    static const uint32_t SUCCESS               = 0;
    static const uint32_t INVALID_PORT_NUMBER   = 1;
    static const uint32_t INVALID_MESSAGE       = 2;
    static const uint32_t BUFFER_TOO_SMALL      = 3;
    static const uint32_t EMPTY_PORT            = 4;


    uint32_t                number;
    LinkedList<Message_t*>  messages;

    uint32_t send(const Message_t* message, Process* sender);
    uint32_t read(Message_t* message);
    bool empty();

    static uint32_t checksum(const void* buffer, uint32_t size);
};

#endif //_PORT_H_


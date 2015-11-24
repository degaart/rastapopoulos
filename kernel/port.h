#ifndef _PORT_H_
#define _PORT_H_

#include <stdint.h>
#include "linked_list.h"
#include "message.h"

class Process;

class Port {
public:
    uint32_t                number;
    LinkedList<Message_t>   messages;

    void send(const Message_t& message, Process* sender);
    int32_t read(Message_t* message);
    bool empty();
};

#endif //_PORT_H_


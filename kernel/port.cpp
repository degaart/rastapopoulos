#include "port.h"
#include "string.h"
#include "kmalloc.h"
#include "util.h"
#include "regs.h"

void Port::send(const Message_t& message, Process* sender) {
    assert(sender);

    Message_t copy = message;
    copy.payload = kmalloc(message.payload_size);
    memcpy(copy.payload, message.payload, message.payload_size);
    copy.sender = sender;

    messages.append(copy);
}

int32_t Port::read(Message_t* buffer) {
    if(empty())
        return -1;

    Message_t msg = messages.head();
    if(msg.payload_size > buffer->payload_size) {
        return msg.payload_size;
    }
    messages.pop();
    assert(msg.sender);         /* Hell, what happens if sender exits before we process this message? */

    memcpy(buffer, &msg, sizeof(Message_t));
    buffer->id = msg.id;
    buffer->result = msg.result;
    memcpy(buffer->payload, msg.payload, msg.payload_size);
    buffer->payload_size = msg.payload_size;

    kfree(msg.payload);
    return 0;
}

bool Port::empty() {
    return messages.size() == 0;
}





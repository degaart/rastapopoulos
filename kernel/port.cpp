#include "port.h"
#include "string.h"
#include "kmalloc.h"
#include "util.h"
#include "regs.h"
#include "crc32.h"

uint32_t Port::send(const Message_t* message, Process* sender) {
    assert(sender);

    msg_check_checksum(message);

    Message_t* copy = (Message_t*)kmalloc(sizeof(Message_t));
    memset(copy, 0, sizeof(Message_t));
    msg_copy(copy, message);
    messages.append(copy);
    return SUCCESS;
}

uint32_t Port::read(Message_t* buffer) {
    if(empty())
        return EMPTY_PORT;

    Message_t* msg = messages.head();
    if(msg->payload_size > buffer->payload_size) {
        buffer->payload_size = msg->payload_size;
        return BUFFER_TOO_SMALL;
    }
    messages.pop();
    assert(msg->sender);         /* Hell, what happens if sender exits before we process this message? */
    msg_check_checksum(msg);
    msg_copy(buffer, msg);
    kfree(msg->payload);
    return SUCCESS;
}

bool Port::empty() {
    return messages.size() == 0;
}

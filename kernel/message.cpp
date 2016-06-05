#include "message.h"
#include "crc32.h"
#include "debug.h"
#include "string.h"
#include "kmalloc.h"

void msg_update_checksum(Message_t* msg)
{
    msg->payload_checksum = crc_finalize(crc_update(crc_init(), msg->payload, msg->payload_size));
}

void msg_check_checksum(const Message_t* msg)
{
    uint32_t crc = crc_finalize(crc_update(crc_init(), msg->payload, msg->payload_size));
    if(msg->payload_checksum != crc) {
        TRACE("Message checksum mismatch: expected 0x%X, got 0x%X\n", msg->payload_checksum, crc); 
        TRACE("Size: %u bytes", msg->payload_size);
        TRACE("Dump: %s", msg->payload);

        halt();
    }
}

void msg_copy(Message_t* dst, const Message_t* msg)
{
    memcpy(dst, msg, sizeof(Message_t));
    dst->payload = kmalloc(msg->payload_size);
    memcpy(dst->payload, msg->payload, msg->payload_size);
}


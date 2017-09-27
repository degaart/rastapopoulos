#include <stddef.h>
#include "logger_common.h"
#include "logger_server.h"

#if __STDC_HOSTED__
#  include <stdlib.h>
#  include <assert.h>
#  include <string.h>
#  define panic(s) assert(!(s))
#else
#  include <debug.h>
#  include <port.h>
#  include <string.h>
#  include <util.h>
#  ifdef KERNEL
#      include "kmalloc.h"
#      define malloc(s) kmalloc(s)
#      define free(s) kfree(s)
#  else
#      include <malloc.h>
#      include <runtime.h>
#  endif
#endif

static size_t marshall_logger_trace(int sender_pid, void* resbuf, size_t resbuf_size, const void* argbuf, size_t argbuf_size)
{
	/* Deserialize arguments */
	const unsigned char* in_ptr = argbuf;
	size_t in_size = argbuf_size;
	char typecode;

	/* Deserialize msg */
	assert(in_size >= 1); /* msg typecode */
	typecode = *((const char*)in_ptr);
	assert(typecode == 's');
	in_ptr++;
	in_size--;

	assert(in_size >= sizeof(size_t));
	size_t arg_msg_size = *((const size_t*)in_ptr);
	in_ptr += sizeof(size_t);
	in_size -= sizeof(size_t);
	assert(in_size >= arg_msg_size);
	const char* arg_msg = (const char*)in_ptr;
	in_ptr += arg_msg_size;
	in_size -= arg_msg_size;


	/* Call handler */
	handle_logger_trace(sender_pid, arg_msg);

	return -1;
}

void rpc_dispatch(int port)
{
	struct message* snd_buf = malloc(4096);
	struct message* rcv_buf = malloc(4096);
	size_t snd_buf_size = 4096 - sizeof(struct message);
	unsigned outsize;

	while(1) {
		int ret = msgrecv(port, rcv_buf, 4096, &outsize);
		if(ret)
			panic("msgrecv() failed");

		size_t result;
		switch(rcv_buf->code) {
			case MSG_LOGGER_TRACE:
				result = marshall_logger_trace(
					rcv_buf->sender,
					snd_buf->data, snd_buf_size,
					rcv_buf->data, rcv_buf->len);
				break;
			default:
				panic("Invalid message code 0x%X from %d", rcv_buf->code, rcv_buf->sender);
		}

		if(result != -1) {
			snd_buf->len = result;
			snd_buf->reply_port = INVALID_PORT;
			snd_buf->code = MSG_NULL;
			ret = msgsend(rcv_buf->reply_port, snd_buf);
			if(ret)
				panic("msgsend() failed");
		}
	}
}


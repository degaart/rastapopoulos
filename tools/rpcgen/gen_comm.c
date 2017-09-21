#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include "rpc_types.h"
#include "rpcgen.h"


static void generate_messagecodes(const struct RPCStatement* statements)
{
    fprintf(_files.comm.h, "/* Message codes */\n");

    int max_width = strlen("NULL");
    for(const struct RPCStatement* st = statements; st; st = st->next) {
        if(st->type == RPCFunctionStatement) {
            size_t len = strlen(st->fn->name);
            if(len > max_width)
                max_width = len;
        }
    }

    max_width += 4;
    fprintf(_files.comm.h,
            "#ifndef MSG_NULL\n"
            "#define MSG_%-*s 0x0000\n"
            "#endif\n"
            "\n",
            max_width,
            "NULL");

    int message_idx = 1;
    for(const struct RPCStatement* st = statements; st; st = st->next) {
        if(st->type == RPCFunctionStatement) {
            char* name = strdup(st->fn->name);
            strupper(name);
            fprintf(_files.comm.h,
                    "#define MSG_%-*s 0x%04X\n",
                    max_width,
                    name,
                    message_idx);
            free(name);
            message_idx++;
        }
    }
    fprintf(_files.comm.h,
            "\n");
}

void generate_commonside(const struct RPCStatement* statements)
{
    generate_messagecodes(statements);

    fprintf(_files.comm.h,
            "#ifdef __STDC_HOSTED__\n"
            "struct message {\n"
            "\tint reply_port;             /* Port number to send response to */\n"
            "\tunsigned code;              /* Message code, interpretation depends on receiver */\n"
            "\tunsigned len;               /* Length of data[] (i.e. the header is not included) */\n"
            "\tunsigned char data[];\n"
            "};\n"
            "int msgsend(int port, struct message* msg);\n"
            "int msgrecv(int port, struct message* buffer, unsigned buffer_size, unsigned* outsize);\n"
            "#define INVALID_PORT 0\n"
            "#else /* __STDC_HOSTED__ */\n"
            "#include <port.h>\n"
            "#endif\n"
            "\n");
}



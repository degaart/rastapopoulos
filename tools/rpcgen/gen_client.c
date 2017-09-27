#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include "rpc_types.h"
#include "rpcgen.h"

static void generate_client_marshaller_declaration(FILE* file,
                                                   const struct RPCStatement* statements,
                                                   const struct RPCFunction* fn)
{
    fprintf(file, "int %s(\n", fn->name);

    switch(fn->return_type->type) {
        case VoidType:
            fprintf(file,
                    "\tint rpc_port,\n"
                    "\tint rpc_reply_port");
            break;
        case IntType:
            fprintf(file,
                    "\tint* rpc_result,\n"
                    "\tint rpc_port,\n"
                    "\tint rpc_reply_port");
            break;
        case StringType:
            panic("Invalid function return type for %s: void",
                  fn->name);
            break;
        case LongType:
            fprintf(file,
                    "\tlong long* rpc_result,\n"
                    "\tint rpc_port,\n"
                    "\tint rpc_reply_port");
            break;
        case BlobType:
            panic("Invalid function return type for %s: blob",
                  fn->name);
            break;
        default:
            assert(!"Invalid code path");
    }

    for(const struct RPCArgument* arg = fn->args; arg; arg = arg->next) {
        fprintf(file, ", \n");

        if(arg->type->modifier != OutModifier) {
            switch(arg->type->type) {
                case IntType:
                    fprintf(file, "\tint %s", arg->name);
                    break;
                case StringType:
                    fprintf(file, "\tconst char* %s", arg->name);
                    break;
                case LongType:
                    fprintf(file, "\tlong long %s", arg->name);
                    break;
                case BlobType:
                    fprintf(file, "\tconst void* %s, size_t %s_size", 
                            arg->name, arg->name);
                    break;
                default:
                    assert(!"Invalid code path");
            }

        } else {
            switch(arg->type->type) {
                case IntType:
                    fprintf(file, "\t/* out */ int* %s", arg->name);
                    break;
                case StringType:
                    fprintf(file, "\t/* out */ char* %s, /* in */ size_t %s_size", 
                            arg->name, arg->name);
                    break;
                case LongType:
                    fprintf(file, "\t/* out */ long long* %s", arg->name);
                    break;
                case BlobType:
                    fprintf(file, "\t/* out */ void* %s, /* in, out */ size_t* %s_size", 
                            arg->name, arg->name);
                    break;
                default:
                    assert(!"Invalid code path");
            }
        }
    }

    fprintf(file, ")");
    if(fn->return_type->modifier == OneWayModifier)
        fprintf(file, " /* oneway */");
}

static void generate_client_marshaller_argserializer(FILE* file,
                                                     const struct RPCStatement* statements,
                                                     const struct RPCFunction* fn)
{
    fprintf(file,
            "\t/* Determine send buffer size */\n"
            "\tsize_t sndbuf_size = sizeof(struct message);\n");

    for(const struct RPCArgument* arg = fn->args; arg; arg = arg->next) {
        if(arg->type->modifier != OutModifier) {
            switch(arg->type->type) {
                case IntType:
                    fprintf(file, "\tsndbuf_size += 1; /* %s typecode */\n", arg->name);
                    fprintf(file, "\tsndbuf_size += sizeof(int); /* %s */\n", arg->name);
                    break;
                case StringType:
                    fprintf(file,
                            "\tsndbuf_size += 1; /* %s typecode */\n",
                            arg->name);
                    fprintf(file, 
                            "\tsize_t %s_strlen = strlen(%s); /* %s */\n"
                            "\tsndbuf_size += sizeof(size_t); /* %s */\n"
                            "\tsndbuf_size += %s_strlen + 1; /* %s */\n",
                            arg->name, arg->name, arg->name,
                            arg->name, arg->name, arg->name);

                    break;
                case LongType:
                    fprintf(file, "\tsndbuf_size += 1; /* %s typecode */\n", arg->name);
                    fprintf(file, "\tsndbuf_size += sizeof(long long); /* %s */\n", arg->name);
                    break;
                case BlobType:
                    fprintf(file, "\tsndbuf_size += 1; /* %s typecode */\n", arg->name);
                    fprintf(file, 
                            "\tsndbuf_size += sizeof(size_t); /* %s */\n"
                            "\tsndbuf_size += %s_size; /* %s */\n",
                            arg->name, arg->name, arg->name);
                    break;
                default:
                    assert(!"Invalid code path");
            }
        } else {
            switch(arg->type->type) {
                case IntType:
                    break;
                case StringType:
                    fprintf(file, "\tsndbuf_size += 1; /* %s typecode */\n", arg->name);
                    fprintf(file,
                            "\tsndbuf_size += sizeof(size_t); /* %s size */\n",
                            arg->name);
                    break;
                case LongType:
                    break;
                case BlobType:
                    fprintf(file, "\tsndbuf_size += 1; /* %s typecode */\n", arg->name);
                    fprintf(file,
                            "\tsndbuf_size += sizeof(size_t); /* %s size */\n",
                            arg->name);
                    break;
                default:
                    assert(!"Invalid code path");
            }
        }
    }

    fprintf(file,
            "\n"
            "\t/* Allocate buffer and serialize args */\n"
            "\tstruct message* sndbuf = malloc(sndbuf_size);\n"
            "\tunsigned char* snd_ptr = sndbuf->data;\n"
            "\tsize_t snd_rem = sndbuf_size - sizeof(struct message);\n"
            "\n");
    for(const struct RPCArgument* arg = fn->args; arg; arg = arg->next) {
        if(arg->type->modifier != OutModifier) {
            switch(arg->type->type) {
                case IntType:
                    fprintf(file,
                            "\tassert(snd_rem >= 1); /* %s typecode */\n"
                            "\t*((char*)snd_ptr) = 'i'; /* %s typecode */\n"
                            "\tsnd_ptr++; /* %s typecode */\n"
                            "\tsnd_rem--; /* %s typecode */\n",
                            arg->name, arg->name, arg->name, arg->name);
                    fprintf(file, 
                            "\tassert(snd_rem >= sizeof(int));\n"
                            "\t*((int*)snd_ptr) = %s;\n"
                            "\tsnd_ptr += sizeof(int);\n"
                            "\tsnd_rem -= sizeof(int);\n"
                            "\n",
                            arg->name);
                    break;
                case StringType:
                    fprintf(file,
                            "\tassert(snd_rem >= 1); /* %s typecode */\n"
                            "\t*((char*)snd_ptr) = 's'; /* %s typecode */\n"
                            "\tsnd_ptr++; /* %s typecode */\n"
                            "\tsnd_rem--; /* %s typecode */\n",
                            arg->name, arg->name, arg->name, arg->name);
                    fprintf(file, 
                            "\tassert(snd_rem >= sizeof(size_t) + %s_strlen + 1);\n"
                            "\t*((size_t*)snd_ptr) = %s_strlen + 1;\n"
                            "\tmemcpy(snd_ptr + sizeof(size_t), %s, %s_strlen + 1);\n"
                            "\tsnd_ptr += sizeof(size_t) + %s_strlen + 1;\n"
                            "\tsnd_rem -= sizeof(size_t) + %s_strlen + 1;\n"
                            "\n",
                            arg->name, arg->name, arg->name,
                            arg->name, arg->name, arg->name);
                    break;
                case LongType:
                    fprintf(file,
                            "\tassert(snd_rem >= 1); /* %s typecode */\n"
                            "\t*((char*)snd_ptr) = 'l'; /* %s typecode */\n"
                            "\tsnd_ptr++; /* %s typecode */\n"
                            "\tsnd_rem--; /* %s typecode */\n",
                            arg->name, arg->name, arg->name, arg->name);
                    fprintf(file, 
                            "\tassert(snd_rem >= sizeof(long long));\n"
                            "\t*((long long*)snd_ptr) = %s;\n"
                            "\tsnd_ptr += sizeof(long long);\n"
                            "\tsnd_rem -= sizeof(long long);\n"
                            "\n",
                            arg->name);
                    break;
                case BlobType:
                    fprintf(file,
                            "\tassert(snd_rem >= 1); /* %s typecode */\n"
                            "\t*((char*)snd_ptr) = 'b'; /* %s typecode */\n"
                            "\tsnd_ptr++; /* %s typecode */\n"
                            "\tsnd_rem--; /* %s typecode */\n",
                            arg->name, arg->name, arg->name, arg->name);
                    fprintf(file, 
                            "\tassert(snd_rem >= sizeof(size_t) + %s_size);\n"
                            "\t*((size_t*)snd_ptr) = %s_size;\n"
                            "\tmemcpy(snd_ptr + sizeof(size_t), %s, %s_size);\n"
                            "\tsnd_ptr += sizeof(size_t) + %s_size;\n"
                            "\tsnd_rem -= sizeof(size_t) + %s_size;\n"
                            "\n",
                            arg->name, arg->name, arg->name,
                            arg->name, arg->name, arg->name);
                    break;
                default:
                    assert(!"Invalid code path");
            }
        } else {
            switch(arg->type->type) {
                case IntType:
                    break;
                case StringType:
                    fprintf(file,
                            "\tassert(snd_rem >= 1); /* %s typecode */\n"
                            "\t*((char*)snd_ptr) = 'S'; /* %s typecode */\n"
                            "\tsnd_ptr++; /* %s typecode */\n"
                            "\tsnd_rem--; /* %s typecode */\n",
                            arg->name, arg->name, arg->name, arg->name);
                    fprintf(file,
                            "\tassert(snd_rem >= sizeof(size_t));\n"
                            "\t*((size_t*)snd_ptr) = %s_size;\n"
                            "\tsnd_ptr += sizeof(size_t);\n"
                            "\tsnd_rem -= sizeof(size_t);\n"
                            "\n",
                            arg->name);
                    break;
                case LongType:
                    break;
                case BlobType:
                    fprintf(file,
                            "\tassert(snd_rem >= 1); /* %s typecode */\n"
                            "\t*((char*)snd_ptr) = 'B'; /* %s typecode */\n"
                            "\tsnd_ptr++; /* %s typecode */\n"
                            "\tsnd_rem--; /* %s typecode */\n",
                            arg->name, arg->name, arg->name, arg->name);
                    fprintf(file,
                            "\tassert(snd_rem >= sizeof(size_t));\n"
                            "\t*((size_t*)snd_ptr) = *%s_size;\n"
                            "\tsnd_ptr += sizeof(size_t);\n"
                            "\tsnd_rem -= sizeof(size_t);\n"
                            "\n",
                            arg->name);
                    break;
                    break;
                default:
                    assert(!"Invalid code path");
            }
        }
    }

    char* msgcode = strdup(fn->name);
    strupper(msgcode);
    fprintf(file,
            "\n"
            "\tsndbuf->reply_port = rpc_reply_port;\n"
            "\tsndbuf->code = MSG_%s;\n"
            "\tsndbuf->len = sndbuf_size - sizeof(struct message);\n",
            msgcode);
    free(msgcode);
}

static void generate_client_marshaller_resultdeserializer(FILE* file,
                                                          const struct RPCStatement* statements,
                                                          const struct RPCFunction* fn)
{
    fprintf(file,
            "\t/* Determine buffer size for results */\n"
            "\tsize_t rcvbuf_size = sizeof(struct message);\n");
    switch(fn->return_type->type) {
        case VoidType:
            break;
        case IntType:
            fprintf(file, "\trcvbuf_size += 1; /* rpc_result typecode */\n");
            fprintf(file, "\trcvbuf_size += sizeof(int); /* rpc_result */\n");
            break;
        case LongType:
            fprintf(file, "\trcvbuf_size += 1; /* rpc_result typecode */\n");
            fprintf(file, "\trcvbuf_size += sizeof(long long); /* rpc_result */\n");
            break;
        case StringType:
        case BlobType:
            panic("Invalid function result type for %s: %s",
                  fn->name,
                  stringify_type(fn->return_type));
            break;
        default:
            panic("Invalid code path");
    }

    for(const struct RPCArgument* arg = fn->args; arg; arg = arg->next) {
        if(arg->type->modifier == OutModifier) {
            switch(arg->type->type) {
                case IntType:
                    fprintf(file, "\trcvbuf_size += 1; /* %s typecode */\n", arg->name);
                    fprintf(file,
                            "\trcvbuf_size += sizeof(int); /* %s */\n",
                            arg->name);
                    break;
                case StringType:
                    fprintf(file, "\trcvbuf_size += 1; /* %s typecode */\n", arg->name);
                    fprintf(file,
                            "\trcvbuf_size += sizeof(size_t); /* %s */\n"
                            "\trcvbuf_size += %s_size; /* %s size */\n",
                            arg->name, arg->name, arg->name);
                    break;
                case LongType:
                    fprintf(file, "\trcvbuf_size += 1; /* %s typecode */\n", arg->name);
                    fprintf(file,
                            "\trcvbuf_size += sizeof(long long); /* %s */\n",
                            arg->name);
                    break;
                case BlobType:
                    fprintf(file, "\trcvbuf_size += 1; /* %s typecode */\n", arg->name);
                    fprintf(file,
                            "\trcvbuf_size += sizeof(size_t); /* %s */\n"
                            "\trcvbuf_size += *%s_size; /* %s size */\n",
                            arg->name, arg->name, arg->name);
                    break;
                default:
                    panic("Invalid code path");
            }
        }
    }

    if(fn->return_type->modifier != OneWayModifier ) {
        fprintf(file,
                "\n"
                "\t/* Receive results */\n"
                "\tstruct message* rcvbuf = malloc(rcvbuf_size);\n"
                "\tret = msgrecv(rpc_reply_port, rcvbuf, rcvbuf_size, NULL);\n"
                "\tif(ret != 0)\n"
                "\t\treturn RPC_FAIL_RECV;\n"
                "\n"
                "\tconst unsigned char* rcvbuf_ptr = rcvbuf->data;\n"
                "\tsize_t rcvbuf_rem = rcvbuf->len;\n"
                "\tchar rpc_typecode;\n"
                "\n");
        if(fn->return_type->type != VoidType) {
            fprintf(file,
                    "\t/* deserialize result */\n");
            switch(fn->return_type->type) {
                case IntType:
                    fprintf(file,
                            "\tassert(rcvbuf_rem >= 1);\n"
                            "\trpc_typecode = *((const char*)rcvbuf_ptr);\n"
                            "\tassert(rpc_typecode == 'I');\n"
                            "\trcvbuf_ptr++;\n"
                            "\trcvbuf_rem--;\n");
                    fprintf(file,
                            "\tassert(rcvbuf_rem >= sizeof(int));\n"
                            "\t*rpc_result = *((const int*)rcvbuf_ptr);\n"
                            "\trcvbuf_ptr += sizeof(int);\n"
                            "\trcvbuf_rem -= sizeof(int);\n"
                            "\n");
                    break;
                case LongType:
                    fprintf(file,
                            "\tassert(rcvbuf_rem >= 1);\n"
                            "\trpc_typecode = *((const char*)rcvbuf_ptr);\n"
                            "\tassert(rpc_typecode == 'L');\n"
                            "\trcvbuf_ptr++;\n"
                            "\trcvbuf_rem--;\n");
                    fprintf(file,
                            "\tassert(rcvbuf_rem >= sizeof(long long));\n"
                            "\t*rpc_result = *((const long long*)rcvbuf_ptr);\n"
                            "\trcvbuf_ptr += sizeof(long long);\n"
                            "\trcvbuf_rem -= sizeof(long long);\n"
                            "\n");
                    break;
                default:
                    panic("Invalid code path");
            }
        }

        for(const struct RPCArgument* arg = fn->args; arg; arg = arg->next) {
            if(arg->type->modifier == OutModifier) {
                switch(arg->type->type) {
                    case IntType:
                        fprintf(file,
                                "\tassert(rcvbuf_rem >= 1);\n"
                                "\trpc_typecode = *((const char*)rcvbuf_ptr);\n"
                                "\tassert(rpc_typecode == 'I');\n"
                                "\trcvbuf_ptr++;\n"
                                "\trcvbuf_rem--;\n");
                        fprintf(file,
                                "\tassert(rcvbuf_rem >= sizeof(int));\n"
                                "\t*%s = *((const int*)rcvbuf_ptr);\n"
                                "\trcvbuf_ptr += sizeof(int);\n"
                                "\trcvbuf_rem -= sizeof(int);\n"
                                "\n",
                                arg->name);
                        break;
                    case StringType:
                        fprintf(file,
                                "\tassert(rcvbuf_rem >= 1);\n"
                                "\trpc_typecode = *((const char*)rcvbuf_ptr);\n"
                                "\tassert(rpc_typecode == 'S');\n"
                                "\trcvbuf_ptr++;\n"
                                "\trcvbuf_rem--;\n");
                        fprintf(file,
                                "\tassert(rcvbuf_rem >= sizeof(size_t));\n"
                                "\tsize_t %s_outsize = *((const size_t*)rcvbuf_ptr);\n"
                                "\trcvbuf_ptr += sizeof(size_t);\n"
                                "\trcvbuf_rem -= sizeof(size_t);\n"
                                "\tassert(%s_size >= %s_outsize);\n"
                                "\tassert(rcvbuf_rem >= %s_outsize);\n"
                                "\tmemcpy(%s, (const char*)rcvbuf_ptr, %s_outsize);\n"
                                "\trcvbuf_ptr += %s_outsize;\n"
                                "\trcvbuf_rem -= %s_outsize;\n"
                                "\n",
                                arg->name, arg->name, arg->name,
                                arg->name, arg->name, arg->name,
                                arg->name, arg->name);
                        break;
                    case LongType:
                        fprintf(file,
                                "\tassert(rcvbuf_rem >= 1);\n"
                                "\trpc_typecode = *((const char*)rcvbuf_ptr);\n"
                                "\tassert(rpc_typecode == 'L');\n"
                                "\trcvbuf_ptr++;\n"
                                "\trcvbuf_rem--;\n");
                        fprintf(file,
                                "\tassert(rcvbuf_rem >= sizeof(long long));\n"
                                "\t*%s = *((const long long*)rcvbuf_ptr);\n"
                                "\trcvbuf_ptr += sizeof(long long);\n"
                                "\trcvbuf_rem -= sizeof(long long);\n"
                                "\n",
                                arg->name);
                        break;
                    case BlobType:
                        fprintf(file,
                                "\tassert(rcvbuf_rem >= 1);\n"
                                "\trpc_typecode = *((const char*)rcvbuf_ptr);\n"
                                "\tassert(rpc_typecode == 'B');\n"
                                "\trcvbuf_ptr++;\n"
                                "\trcvbuf_rem--;\n");
                        fprintf(file,
                                "\tassert(rcvbuf_rem >= sizeof(size_t));\n"
                                "\tsize_t received_%s_size = *((const size_t*)rcvbuf_ptr);\n"
                                "\trcvbuf_ptr += sizeof(size_t);\n"
                                "\trcvbuf_rem -= sizeof(size_t);\n"
                                "\tassert(rcvbuf_rem >= received_%s_size);\n"
                                "\tassert(*%s_size >= received_%s_size);\n"
                                "\tmemcpy(%s, (const char*)rcvbuf_ptr, received_%s_size);\n"
                                "\trcvbuf_ptr += received_%s_size;\n"
                                "\trcvbuf_rem -= received_%s_size;\n"
                                "\t*%s_size = received_%s_size;\n"
                                "\n",
                                arg->name, arg->name, arg->name,
                                arg->name, arg->name, arg->name,
                                arg->name, arg->name,
                                arg->name, arg->name);
                        break;
                    default:
                        panic("Invalid code path");
                }
            }
        }

        fprintf(file,
                "\tfree(rcvbuf);\n"
                "\n");
    }
}

static void generate_client_marshaller(const struct RPCStatement* statements,
                                       const struct RPCFunction* fn)
{
    /* function declaration */
    generate_client_marshaller_declaration(_files.clt.c, statements, fn);
    fprintf(_files.clt.c, 
            "\n"
            "{\n");

    generate_client_marshaller_argserializer(_files.clt.c, statements, fn);
    fprintf(_files.clt.c,
            "\n"
            "\tint ret = msgsend(rpc_port, sndbuf);\n"
            "\tif(ret != 0)\n"
            "\t\treturn RPC_FAIL_SEND;\n"
            "\n"
            "\tfree(sndbuf);\n"
            "\n");

    if(fn->return_type->modifier != OneWayModifier) {
        generate_client_marshaller_resultdeserializer(_files.clt.c, statements, fn);
    } else {
        if(fn->return_type->type != VoidType) {
            panic("Invalid return type for oneway function %s: %s",
                  fn->name,
                  stringify_type(fn->return_type));
        }
    }

    fprintf(_files.clt.c,
            "\treturn RPC_OK;\n"
            "}\n"
            "\n");

    generate_client_marshaller_declaration(_files.clt.h, statements, fn);
    fprintf(_files.clt.h,
            ";\n");
}

void generate_clientside(const struct RPCStatement* statements)
{
    fprintf(_files.clt.c,
            "#include <stddef.h>\n"
            "#include \"%s\"\n"
            "#include \"%s\"\n"
            "\n"
            "#if __STDC_HOSTED__\n"
            "#include <stdlib.h>\n"
            "#include <assert.h>\n"
            "#include <string.h>\n"
            "#define panic(s) assert(!(s))\n"
            "#else\n"
            "#include <string.h>\n"
            "#include <malloc.h>\n"
            "#include <runtime.h>\n"
            "#include <debug.h>\n"
            "#endif\n"
            "\n"
            "\n",
            _files.comm.hf, _files.clt.hf);

    for(const struct RPCStatement* st = statements; st; st = st->next) {
        if(st->type == RPCFunctionStatement) {
            generate_client_marshaller(statements, st->fn);
        }
    }

    fprintf(_files.clt.c,
            "\n");
    fprintf(_files.clt.h,
            "\n");

    fprintf(_files.clt.h,
            "/* Rpc client result codes */\n"
            "#ifndef RPC_OK\n"
            "#  define RPC_OK           0\n"
            "#  define RPC_FAIL_SEND    1\n"
            "#  define RPC_FAIL_RECV    2\n"
            "#endif\n"
            "\n\n");
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include "rpc_types.h"
#include "rpcgen.h"

static void generate_result_serializer(FILE* file,
                                       const struct RPCStatement* statements,
                                       const struct RPCFunction* fn)
{
    int has_result = 0;
    if(fn->return_type->type != VoidType)
        has_result++;

    for(const struct RPCArgument* arg = fn->args; arg; arg = arg->next) {
        if(arg->type->modifier == OutModifier) {
            has_result++;
        }
    }

    if(has_result) {
        fprintf(file, "\t/* Serialize result */\n"
                      "\tunsigned char* result_ptr = (unsigned char*)resbuf;\n"
                      "\tsize_t avail = resbuf_size;\n"
                      "\n"); 

        switch(fn->return_type->type) {
            case VoidType:
                break;
            case IntType:
                fprintf(file, 
                        "\tassert(avail >= sizeof(int));\n"
                        "\t*((int*)result_ptr) = fn_result;\n"
                        "\tresult_ptr += sizeof(int);\n"
                        "\tavail -= sizeof(int);\n"
                        "\n");
                break;
            case LongType:
                fprintf(file, 
                        "\tassert(avail >= sizeof(long long));\n"
                        "\t*((int*)result_ptr) = fn_result;\n"
                        "\tresult_ptr += sizeof(long long);\n"
                        "\tavail -= sizeof(long long);\n"
                        "\n");
                break;
            default:
                assert(!"Invalid code path");
        }

        int arg_index = 0;
        for(const struct RPCArgument* arg = fn->args; arg; arg = arg->next, arg_index++) {
            if(arg->type->modifier == OutModifier) {
                switch(arg->type->type) {
                    case IntType:
                        fprintf(_files.srv.c,
                                "\t/* Serialize %s */\n"
                                "\tassert(avail >= sizeof(int));\n"
                                "\t*((int*)result_ptr) = out_%s;\n"
                                "\tresult_ptr += sizeof(int);\n"
                                "\tavail -= sizeof(int);\n"
                                "\n",
                                arg->name, arg->name);
                        break;
                    case StringType:
                        fprintf(_files.srv.c, 
                                "\t/* Serialize %s */\n"
                                "\tassert(avail >= sizeof(size_t));\n"
                                "\tout_%s_size = strlen(out_%s) + 1;\n"
                                "\t*((size_t*)result_ptr) = out_%s_size;\n"
                                "\tresult_ptr += sizeof(size_t);\n"
                                "\tavail -= sizeof(size_t);\n"
                                "\tassert(avail >= out_%s_size);\n"
                                "\tmemcpy(result_ptr, out_%s, out_%s_size);\n"
                                "\tresult_ptr += out_%s_size;\n"
                                "\tavail -= out_%s_size;\n"
                                "\tfree(out_%s);\n"
                                "\n",
                                arg->name,
                                arg->name, arg->name, arg->name,
                                arg->name, arg->name, arg->name,
                                arg->name, arg->name, arg->name);
                        break;
                    case LongType:
                        fprintf(_files.srv.c, 
                                "\t/* Serialize %s */\n"
                                "\tassert(avail >= sizeof(long long));\n"
                                "\t*((long long*)result_ptr) = out_%s;\n"
                                "\tresult_ptr += sizeof(long long);\n"
                                "\tavail -= sizeof(long long);\n"
                                "\n",
                                arg->name, arg->name);
                        break;
                    case BlobType:
                        fprintf(_files.srv.c, 
                                "\t/* Serialize %s */\n"
                                "\tassert(avail >= sizeof(size_t));\n"
                                "\t*((size_t*)result_ptr) = out_%s_size;\n"
                                "\tresult_ptr += sizeof(size_t);\n"
                                "\tavail -= sizeof(size_t);\n"
                                "\tassert(avail >= out_%s_size);\n"
                                "\tmemcpy(result_ptr, out_%s, out_%s_size);\n"
                                "\tresult_ptr += out_%s_size;\n"
                                "\tavail -= out_%s_size;\n"
                                "\tfree(out_%s);\n"
                                "\n",
                                arg->name,
                                arg->name, arg->name, arg->name,
                                arg->name, arg->name, arg->name,
                                arg->name);
                        break;
                    default:
                        fprintf(stderr, "Unsupported output argument type: %s",
                                stringify_type(arg->type));
                        abort();
                        break;
                }
            }
        }

        fprintf(_files.srv.c, "\tsize_t result = resbuf_size - avail;\n");
        fprintf(_files.srv.c, "\treturn result;\n");
    } else {
        if(fn->return_type->modifier == OneWayModifier)
            fprintf(_files.srv.c, "\treturn -1;\n");
        else
            fprintf(_files.srv.c, "\treturn 0;\n");
    }
}

static void generate_handler_call(FILE* file,
                                  const struct RPCStatement* statements,
                                  const struct RPCFunction* fn)
{
    fprintf(file,
            "\t/* Call handler */\n");

    /* variable declarations for out parameters */
    int arg_index = 0;
    for(const struct RPCArgument* arg = fn->args; arg; arg = arg->next, arg_index++) {
        if(arg->type->modifier == OutModifier) {
            switch(arg->type->type) {
                case IntType:
                    fprintf(file, "\tint out_%s = 0;\n", arg->name);
                    break;
                case StringType:
                    fprintf(file, 
                            "\tchar* out_%s = malloc(out_%s_size);\n",
                            arg->name, arg->name);
                    break;
                case LongType:
                    fprintf(file, 
                            "\tlong long out_%s = 0;\n", 
                            arg->name);
                    break;
                case BlobType:
                    fprintf(file, 
                            "\tvoid* out_%s = malloc(out_%s_size);\n",
                            arg->name, arg->name);
                    break;
                default:
                    assert(!"Invalid code path");
            }
        }
    }

    switch(fn->return_type->type) {
        case VoidType:
            fprintf(_files.srv.c, "\thandle_%s(", fn->name);
            break;
        case IntType:
            fprintf(_files.srv.c, "\tint fn_result = handle_%s(", fn->name);
            break;
        case LongType:
            fprintf(_files.srv.c, "\tlong long fn_result = handle_%s(", fn->name);
            break;
        default:
            assert(!"Invalid code path");
    }

    arg_index = 0;
    for(const struct RPCArgument* arg = fn->args; arg; arg = arg->next, arg_index++) {
        if(arg->type->modifier == NullModifier) {
            if(arg_index)
                fprintf(_files.srv.c, ", ");

            switch(arg->type->type) {
                case IntType:
                case LongType:
                case StringType:
                    fprintf(_files.srv.c, "arg_%s", arg->name);
                    break;
                case BlobType:
                    fprintf(_files.srv.c, "arg_%s, arg_%s_size", 
                            arg->name, arg->name);
                    break;
                default:
                    panic("Invalid code path");
            }

        } else if(arg->type->modifier == OutModifier) {
            if(arg_index)
                fprintf(_files.srv.c, ", ");

            switch(arg->type->type) {
                case IntType:
                case LongType:
                    fprintf(_files.srv.c, "&out_%s", arg->name);
                    break;
                case StringType:
                    fprintf(_files.srv.c, "out_%s, out_%s_size", arg->name, arg->name);
                    arg_index++;
                    break;
                case BlobType:
                    fprintf(_files.srv.c, "out_%s, &out_%s_size", arg->name, arg->name);
                    arg_index++;
                    break;
                default:
                    assert(!"Invalid code path");
            }
        }
    }
    fprintf(_files.srv.c, 
            ");\n\n");
}

static void generate_arg_deserializer(FILE* file,
                                      const struct RPCStatement* statements,
                                      const struct RPCFunction* fn)
{
    int has_arguments = (fn->args != NULL);
    if(!has_arguments)
        return;

    fprintf(file, 
            "\t/* Deserialize arguments */\n"
            "\tconst unsigned char* in_ptr = argbuf;\n"
            "\tsize_t in_size = argbuf_size;\n\n");

    for(const struct RPCArgument* arg = fn->args; arg; arg = arg->next) {
        if(arg->type->modifier == NullModifier) {
            fprintf(_files.srv.c, "\t/* Deserialize %s */\n", arg->name);

            switch(arg->type->type) {
                case IntType:
                    fprintf(_files.srv.c, 
                            "\tassert(in_size >= sizeof(int));\n"
                            "\tint arg_%s = *((const int*)in_ptr);\n"
                            "\tin_ptr += sizeof(int);\n"
                            "\tin_size -= sizeof(int);\n"
                            "\n",
                            arg->name);
                    break;
                case StringType:
                    fprintf(_files.srv.c, 
                            "\tassert(in_size >= sizeof(size_t));\n"
                            "\tsize_t arg_%s_size = *((const size_t*)in_ptr);\n"
                            "\tin_ptr += sizeof(size_t);\n"
                            "\tin_size -= sizeof(size_t);\n"
                            "\tassert(in_size >= arg_%s_size);\n"
                            "\tconst char* arg_%s = (const char*)in_ptr;\n"
                            "\tin_ptr += arg_%s_size;\n"
                            "\tin_size -= arg_%s_size;\n"
                            "\n",
                            arg->name,
                            arg->name,
                            arg->name,
                            arg->name,
                            arg->name);
                    break;
                case LongType:
                    fprintf(_files.srv.c, 
                            "\tassert(in_size >= sizeof(long long));\n"
                            "\tlong long arg_%s = *((const long long*)in_ptr);\n"
                            "\tin_ptr += sizeof(long long);\n"
                            "\tin_size -= sizeof(long long);\n"
                            "\n",
                            arg->name);
                    break;
                case BlobType:
                    fprintf(_files.srv.c, 
                            "\tassert(in_size >= sizeof(size_t));\n"
                            "\tsize_t arg_%s_size = *((const size_t*)in_ptr);\n"
                            "\tin_ptr += sizeof(size_t);\n"
                            "\tin_size -= sizeof(size_t);\n"
                            "\tassert(in_size >= arg_%s_size);\n"
                            "\tconst void* arg_%s = (const void*)in_ptr;\n"
                            "\tin_ptr += arg_%s_size;\n"
                            "\tin_size -= arg_%s_size;\n"
                            "\n",
                            arg->name,
                            arg->name,
                            arg->name,
                            arg->name,
                            arg->name);
                    break;
                default:
                    fprintf(stderr, "Invalid function argument type: %s\n",
                            stringify_type(arg->type));
                    abort();
            }
        } else {
            switch(arg->type->type) {
                case IntType:
                    break;
                case StringType:
                    fprintf(file,
                            "\t/* Deserialize buffer size for string %s */\n"
                            "\tassert(in_size >= sizeof(size_t));\n"
                            "\tsize_t out_%s_size = *((const size_t*)in_ptr);\n"
                            "\n",
                            arg->name, arg->name);
                    break;
                case LongType:
                    break;
                case BlobType:
                    fprintf(file,
                            "\t/* Deserialize buffer size for blob %s */\n"
                            "\tassert(in_size >= sizeof(size_t));\n"
                            "\tsize_t out_%s_size = *((const size_t*)in_ptr);\n"
                            "\n",
                            arg->name, arg->name);
                    break;
                default:
                    panic("Invalid code path");
            }
        }
    }
    fprintf(file, "\n");
}

static void generate_marshaller_decl(FILE* file,
                                     const struct RPCStatement* statements,
                                     const struct RPCFunction* fn)
{
    fprintf(_files.srv.c, 
            "static size_t marshall_%s("
            "void* resbuf, "
            "size_t resbuf_size, "
            "const void* argbuf, "
            "size_t argbuf_size"
            ")\n",
            fn->name);
}

static void generate_marshaller(const struct RPCStatement* statements,
                                const struct RPCFunction* fn)
{
    generate_marshaller_decl(_files.srv.c, statements, fn);

    fprintf(_files.srv.c, "{\n");

    generate_arg_deserializer(_files.srv.c, statements, fn);

    generate_handler_call(_files.srv.c, statements, fn);

    generate_result_serializer(_files.srv.c, statements, fn);

    /* Generate error handler */
    fprintf(_files.srv.c, "}\n\n");
}

static void generate_marshallers(const struct RPCStatement* statements)
{
    for(const struct RPCStatement* st = statements; st; st = st->next) {
        if(st->type == RPCFunctionStatement) {
            generate_marshaller(statements, st->fn);
        }
    }
}

static void generate_dispatcher(const struct RPCStatement* statements)
{
    /* Dispatcher */
    fprintf(_files.srv.c, 
            "void dispatch(int port)\n"
            "{\n"
            "\tstruct message* snd_buf = malloc(4096);\n"
            "\tstruct message* rcv_buf = malloc(4096);\n"
            "\tsize_t snd_buf_size = 4096 - sizeof(struct message);\n"
            "\tunsigned outsize;\n"
            "\n"
            "\twhile(1) {\n"
            "\t\tint ret = msgrecv(port, rcv_buf, 4096, &outsize);\n"
            "\t\tif(ret)\n"
            "\t\t\tpanic(\"msgrecv() failed\");\n"
            "\n"
            "\t\tsize_t result;\n"
            "\t\tswitch(rcv_buf->code) {\n");

    for(const struct RPCStatement* st = statements; st; st = st->next) {
        if(st->type == RPCFunctionStatement) {
            char* uname = strdup(st->fn->name);
            strupper(uname);
            fprintf(_files.srv.c, 
                    "\t\t\tcase MSG_%s:\n"
                    "\t\t\t\tresult = marshall_%s(snd_buf->data, snd_buf_size, rcv_buf->data, rcv_buf->len);\n"
                    "\t\t\t\tbreak;\n",
                   uname,
                   st->fn->name);
            free(uname);
        }
    }
    fprintf(_files.srv.c, 
            "\t\t\tdefault:\n"
            "\t\t\t\tpanic(\"Invalid message code\");\n");
    fprintf(_files.srv.c, 
            "\t\t}\n"
            "\n");

    fprintf(_files.srv.c, 
            "\t\tif(result != -1) {\n"
            "\t\t\tsnd_buf->len = result;\n"
            "\t\t\tsnd_buf->reply_port = INVALID_PORT;\n"
            "\t\t\tsnd_buf->code = MSG_NULL;\n"
            "\t\t\tret = msgsend(rcv_buf->reply_port, snd_buf);\n"
            "\t\t\tif(ret)\n"
            "\t\t\t\tpanic(\"msgsend() failed\");"
            "\n"
            "\t\t}\n"
            "\t}\n"
            "}\n\n");
}

static void generate_declaration(const struct RPCStatement* statements,
                                 const struct RPCFunction* fn)
{
    switch(fn->return_type->type) {
        case VoidType:
            fprintf(_files.srv.h, "void handle_%s(", fn->name);
            break;
        case IntType:
            fprintf(_files.srv.h, "int handle_%s(", fn->name);
            break;
        case StringType:
            panic("Invalid function result type");
            break;
        case LongType:
            fprintf(_files.srv.h, "long long handle_%s(", fn->name);
            break;
        case BlobType:
            panic("Invalid function result type");
            break;
        default:
            assert(!"Invalid code path");
    }

    int arg_index = 0;
    for(const struct RPCArgument* arg = fn->args; arg; arg = arg->next, arg_index++) {
        if(arg->type->modifier == NullModifier) {
            if(arg_index)
                fprintf(_files.srv.h, ", ");

            switch(arg->type->type) {
                case IntType:
                    fprintf(_files.srv.h, "int %s", arg->name);
                    break;
                case StringType:
                    fprintf(_files.srv.h, "const char* %s", arg->name);
                    break;
                case LongType:
                    fprintf(_files.srv.h, "long long %s", arg->name);
                    break;
                case BlobType:
                    fprintf(_files.srv.h, "const void* %s, size_t %s_size", arg->name, arg->name);
                    break;
                default:
                    assert(!"Invalid code path");
            }
        } else if(arg->type->modifier == OutModifier) {
            if(arg_index)
                fprintf(_files.srv.h, ", ");

            switch(arg->type->type) {
                case IntType:
                    fprintf(_files.srv.h, "/* out */ int* %s", arg->name);
                    break;
                case StringType:
                    fprintf(_files.srv.h, "/* out */ char* %s, /* in */ size_t %s_size", 
                            arg->name, arg->name);
                    arg_index++;
                    break;
                case LongType:
                    fprintf(_files.srv.h, "/* out */ long long* %s", arg->name);
                    break;
                case BlobType:
                    fprintf(_files.srv.h, "/* out */ void* %s, ", arg->name);
                    fprintf(_files.srv.h, "/* in, out */ size_t* %s_size", arg->name);
                    arg_index++;
                    break;
                default:
                    assert(!"Invalid code path");
            }
        }
    }

    fprintf(_files.srv.h, ");\n");
}

static void generate_declarations(const struct RPCStatement* statements)
{
    /* Handlers declaration */
    fprintf(_files.srv.h, "/* Handlers */\n");
    for(const struct RPCStatement* st = statements; st; st = st->next) {
        if(st->type == RPCFunctionStatement) {
            generate_declaration(statements, st->fn);
        }
    }
    fprintf(_files.srv.h, "\n");

    fprintf(_files.srv.h,
            "/* dispatcher */\n"
            "void dispatch(int);\n"
            "\n");
}

void generate_serverside(const struct RPCStatement* statements)
{
    generate_declarations(statements);

    fprintf(_files.srv.c,
            "#include <stddef.h>\n"
            "#include \"%scommon.h\"\n"
            "#include \"%sserver.h\"\n"
            "\n"
            "#ifdef __STDC_HOSTED__\n"
            "#include <stdlib.h>\n"
            "#include <assert.h>\n"
            "#include <string.h>\n"
            "#define panic(s) assert(!(s))\n"
            "#endif\n"
            "\n",
            _files.prefix, _files.prefix);
    generate_marshallers(statements);
    generate_dispatcher(statements);
}



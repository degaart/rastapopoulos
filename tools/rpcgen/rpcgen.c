#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include "rpc_types.h"
#include "rpcgen.h"

struct files _files = {0};

void __panic(const char* file, int line, const char* function, 
             const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "Error (%s:%d): ",
            function, line);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);

    exit(1);
}

char* strupper(char* str)
{
    while(*str) {
        *str = toupper(*str);
        str++;
    }
    return str;
}

struct stream {
    char* buffer;
    size_t buffer_size;
    size_t pos;
};

static int stream_create(struct stream* s, FILE* f)
{
    s->buffer_size = 512;
    s->pos = 0;
    s->buffer = malloc(s->buffer_size);

    while(1) {

        if(s->buffer_size - s->pos < 512) {
            s->buffer_size += 512;
            s->buffer = realloc(s->buffer, s->buffer_size);
        }

        ssize_t read_bytes = fread(s->buffer + s->pos,
                                   1,
                                   512,
                                   f);
        if(read_bytes == -1) {
            free(s->buffer);
            s->buffer_size = 0;
            s->pos = 0;
            return -1;
        } else if(read_bytes == 0) {
            break;
        }

        s->pos += read_bytes;
    }

    if(s->buffer_size - s->pos < 1) {
        s->buffer_size += 512;
        s->buffer = realloc(s->buffer, s->buffer_size);
    }

    *(s->buffer + s->pos) = '\0';
    s->buffer_size = s->pos;
    s->pos = 0;

    return s->buffer_size;
}

static int peek_char(struct stream* s)
{
    return *(s->buffer + s->pos);
}

static int next_char(struct stream* s)
{
    int ch = peek_char(s);
    if(ch) {
        s->pos++;
        return ch;
    } else {
        return 0;
    }
}

static void skip_whitespace(struct stream* s)
{
    int ch;
    
    while(1) {
        size_t old_pos = s->pos;

        ch = peek_char(s);
        if(ch == '/') {
            next_char(s);
            ch = peek_char(s);
            if(ch == '*') {
                /* Skip to end of comment */
                while((ch = next_char(s))) {
                    if(ch == '*' && peek_char(s) == '/')
                        break;
                }

                if(ch == '*')
                    next_char(s);
            } else {
                /* rollback and exit */
                s->pos = old_pos;
                return;
            }
        } else if(ch > ' ') {
            break;
        } else if(!ch) {
            break;
        }

        next_char(s);
    }
}

static int next_word(struct stream* s,
                     char* buffer,
                     size_t size)
{
    skip_whitespace(s);

    int ch = peek_char(s);
    switch(ch) {
        case '(':
        case ')':
        case '{':
        case '}':
        case ';':
        case ',':
            next_char(s);
            snprintf(buffer, size, "%c", ch);

            return 1;
        case 0:
            return 0;
    }

    next_char(s);
    *(buffer++) = ch;
    size--;

    while( (size > 1) && (ch = peek_char(s)) ) {
        int num = ch >= '0' && ch <= '9';
        int lowal = ch >= 'a' && ch <= 'z';
        int upal = ch >= 'A' && ch <= 'Z';
        int under = ch == '_';

        if(!num && !lowal && !upal && !under)
            break;

        *(buffer++) = next_char(s);
        size--;
    }

    *(buffer++) = '\0';
    size--;

    return 1;
}

static int next_token(struct stream* s,
                      struct token* tok)
{
    int ret = next_word(s, tok->text, sizeof(tok->text));
    if(ret) {
        if(!strcmp(tok->text, "(")) {
            tok->type = LBraceToken;
        } else if(!strcmp(tok->text, ")")) {
            tok->type = RBraceToken;
        } else if(!strcmp(tok->text, "{")) {
            tok->type = LBracketToken;
        } else if(!strcmp(tok->text, "}")) {
            tok->type = RBracketToken;
        } else if(!strcmp(tok->text, ";")) {
            tok->type = SemicolonToken;
        } else if(!strcmp(tok->text, ",")) {
            tok->type = ColonToken;
        } else if(!strcmp(tok->text, "oneway")) {
            tok->type = ModifierToken;
            tok->modifier = OneWayModifier;
        } else if(!strcmp(tok->text, "out")) {
            tok->type = ModifierToken;
            tok->modifier = OutModifier;
        } else if(!strcmp(tok->text, "void")) {
            tok->type = TypenameToken;
            tok->type_name = VoidType;
        } else if(!strcmp(tok->text, "int")) {
            tok->type = TypenameToken;
            tok->type_name = IntType;
        } else if(!strcmp(tok->text, "string")) {
            tok->type = TypenameToken;
            tok->type_name = StringType;
        } else if(!strcmp(tok->text, "long")) {
            tok->type = TypenameToken;
            tok->type_name = LongType;
        } else if(!strcmp(tok->text, "blob")) {
            tok->type = TypenameToken;
            tok->type_name = BlobType;
        } else {
            tok->type = IdentifierToken;
        }
    }
    return ret;
}

static struct token* token_ptr = NULL;

static struct token* peek_token()
{
    return token_ptr;
}

static struct token* read_token()
{
    struct token* result = token_ptr;
    if(result)
        token_ptr = token_ptr->next;
    return result;
}

static void read_identifier(char* buffer, size_t size)
{
    struct token* token = read_token();
    assert(token);
    assert(token->type == IdentifierToken);

    strlcpy(buffer, token->text, size);
}

static int is_type(struct token* token)
{
    return token->type == TypenameToken ||
           token->type == ModifierToken;
}

static struct RPCType* read_type()
{
    struct RPCType* result = alloc(RPCType);
    result->type = NullType;
    result->modifier = NullModifier;

    struct token* read_tokens[64];
    int read_tokens_idx = 0;
    while(1) {
        struct token* token = peek_token();
        assert(token);

        read_tokens[read_tokens_idx++] = token;

        if(token->type == TypenameToken) {
            assert(result->type == NullType);
            result->type = token->type_name;

            read_token();
        } else if(token->type == ModifierToken) {
            assert(result->modifier == NullModifier);
            result->modifier = token->modifier;

            read_token();
        } else {
            break;
        }
    }

    if(result->type == NullType) {
        fprintf(stderr, "Invalid type: ");
        for(int i = 0; i < read_tokens_idx; i++) {
            fprintf(stderr, "%s ", read_tokens[i]->text);
        }
        fprintf(stderr, "\n");
    }

    assert(result->type != NullType);
    return result;
}

static struct RPCArgument* read_function_param()
{
    struct RPCArgument* result = alloc(RPCArgument);

    result->type = read_type();

    struct token* token = read_token();
    assert(token);

    if(token->type != IdentifierToken) {
        fprintf(stderr, "Error, expected IdentifierToken, got %d (%s)\n",
                token->type,
                token->text);
        abort();
    }

    assert(token->type == IdentifierToken);
    strlcpy(result->name, token->text, sizeof(result->name));

    return result;
}

static struct RPCArgument* read_function_param_list()
{
    struct RPCArgument* result = NULL;
    struct RPCArgument* last = NULL;

    while(1) {
        struct token* token = peek_token();
        assert(token);

        /* function-param */
        if(is_type(token)) {
            struct RPCArgument* arg = read_function_param();

            if(!result) {
                result = arg;
                last = arg;
            } else {
                last->next = arg;
                last = arg;
            }

            token = peek_token();
            if(token->type != ColonToken)
                break;

            read_token();
        } else {
            break;
        }
    }
    return result;
}

static struct RPCFunction* read_function()
{
    struct RPCFunction* result = alloc(RPCFunction);

    /* type */
    result->return_type = read_type();
    
    /* IDENTIFIER */
    struct token* name = read_token();
    assert(name);
    assert(name->type == IdentifierToken);
    strlcpy(result->name, name->text, sizeof(result->name));

    /* LBRACE */
    struct token* token = read_token();
    assert(token);
    assert(token->type == LBraceToken);

    /* function-param-list */
    result->args = read_function_param_list();

    /* RBRACE */
    token = read_token();
    assert(token);
    assert(token->type == RBraceToken);

    /* SEMICOLON */
    token = read_token();
    assert(token);
    assert(token->type == SemicolonToken);

    return result;
}

static struct RPCStatement* read_statements()
{
    struct RPCStatement* result = NULL;
    struct RPCStatement* last = NULL;
    while(1) {
        struct token* token = peek_token();
        if(!token)
            break;

        struct RPCStatement* st = alloc(RPCStatement);

        struct RPCFunction* fn = read_function();
        st->type = RPCFunctionStatement;
        st->fn = fn;

        if(!last) {
            result = st;
        } else {
            last->next = st;
        }
        last = st;
    }

    return result;
}

char* stringify_type(const struct RPCType* type)
{
    char* result = malloc(512);
    *result = '\0';

    switch(type->modifier) {
        case OneWayModifier:
            strcat(result, "oneway");
            break;
        case OutModifier:
            strcat(result, "out");
            break;
        case NullModifier:
            break;
    }

    if(*result)
        strcat(result, " ");

    switch(type->type) {
        case NullType:
            strcat(result, "null");
            break;
        case VoidType:
            strcat(result, "void");
            break;
        case IntType:
            strcat(result, "int");
            break;
        case StringType:
            strcat(result, "string");
            break;
        case LongType:
            strcat(result, "long");
            break;
        case BlobType:
            strcat(result, "blob");
            break;
    }

    return result;
}

int main(int argc, char** argv)
{
    if(argc < 4) {
        fprintf(stderr, "Usage: %s <input-file> <output-dir> <prefix>\n",
                argv[0]);
        return 1;
    }

    const char* infile = argv[1];
    const char* outdir = argv[2];
    const char* prefix = argv[3];

    struct stream s;
    FILE* f = fopen(infile, "rt");
    if(!f) {
        fprintf(stderr, "open(\"%s\"): %s\n", infile, strerror(errno));
        return 1;
    }

    int ret = stream_create(&s, f);
    if(ret == -1) {
        perror("read()");
        fclose(f);
        return 1;
    }
    fclose(f);
    f = NULL;

    /* Read all tokens at once */
    struct token* tokens = NULL;
    struct token* last_token = NULL;
    while(1) {
        struct token* token = alloc(token);
        int ret = next_token(&s, token);
        if(!ret)
            break;

        if(last_token) {
            last_token->next = token;
            last_token = token;
        } else {
            last_token = token;
            tokens = token;
        }
    }

    token_ptr = tokens;

    /* read statements one by one */
    struct RPCStatement* statements = read_statements();

    /* open output files */
#define X(a, b) \
    do { \
        size_t filename_size = strlen(outdir) + strlen(prefix) + 64; \
        char* filename = malloc(filename_size); \
        snprintf(filename, filename_size, "%s/%s%s", outdir, prefix, b); \
        a = fopen(filename, "wt"); \
        if(!a) { \
            fprintf(stderr, "open(\"%s\"): %s\n", \
                    filename, \
                    strerror(errno)); \
            return 1; \
        } \
        free(filename); \
    } while(0)

    X(_files.srv.h, "server.h");
    X(_files.srv.c, "server.c");
    X(_files.clt.h, "client.h");
    X(_files.clt.c, "client.c");
    X(_files.comm.h, "common.h");
    X(_files.comm.c, "common.c");
#undef X

    _files.prefix = prefix;

    fprintf(_files.srv.h, "#pragma once\n\n");
    fprintf(_files.clt.h, "#pragma once\n\n");
    fprintf(_files.comm.h, "#pragma once\n\n");

    /* Generate common code */
    generate_commonside(statements);

    /* Generate server-side code */
    generate_serverside(statements);

    /* Generate client-side code */
    generate_clientside(statements);

    /* Close files */
    fclose(_files.srv.h);
    fclose(_files.srv.c);
    fclose(_files.clt.h);
    fclose(_files.clt.c);
    fclose(_files.comm.h);
    fclose(_files.comm.c);

    return 0;
}



/********************************************************
 * Link this with rpc glue code to obtain magic! 
 ********************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "tst_common.h"
#include "tst_server.h"
#include "tst_client.h"

#define SIGNATURE 0xB16B00B5

/******* Handlers code ***********************************************************************/
int handle_ret_int()
{
    printf("[SRV] ret_int => 42\n");
    return 42;
}

long long handle_ret_long()
{
    printf("[SRV] ret_long => 1337\n");
    return 1337;
}

void handle_in_int(int val)
{
    printf("[SRV] in_int(%d)\n",
           val);
}

void handle_in_long(long long val)
{
    printf("[SRV] in_long(%lld)\n",
           val);
}

void handle_in_string(const char* val)
{
    printf("[SRV] in_string(\"%s\")\n",
           val);
}

void handle_in_blob(const void* val, size_t val_size)
{
    printf("[SRV] in_blob(0x");
    for(const unsigned char* ptr = val; ptr < (const unsigned char*)val + val_size; ptr ++) {
        printf("%02X", *ptr);
    }
    printf(")\n");
}

void handle_out_int(/* out */ int* result)
{
    *result = 42;
    printf("[SRV] out_int() => 42\n");
}

void handle_out_long(/* out */ long long* result)
{
    *result = 1337;
    printf("[SRV] out_long() => 1337\n");
}

void handle_out_string(/* out */ char* result, /* in */ size_t result_size)
{
    strlcpy(result, "ph34r my 1337 sk1ll2", result_size);
    printf("[SRV] out_string() => \"%s\"\n", result);
}

void handle_out_blob(/* out */ void* result, /* in, out */ size_t* result_size)
{
    assert(*result_size >= sizeof(uint32_t));
    *((uint32_t*)result) = 0xDEADBEEF;
    *result_size = sizeof(uint32_t);
    
    printf("[SRV] out_blob(0xDEADBEEF)\n");
}

void handle_ow(const char* str)
{
    printf("[SRV] ow(\"%s\")\n", str);
}

/******* Common code ************************************************************************/
static int _socket = -1;

static ssize_t read_all(int fd, void* buffer, size_t size)
{
    ssize_t ret = 0;
    while(size) {
        ssize_t read_bytes = read(fd, buffer, size);
        if(read_bytes == -1 && errno != EINTR) {
            return -1;
        } else if(read_bytes == 0) {
            return 0;
        } else if(read_bytes != -1) {
            buffer = (unsigned char*)buffer + read_bytes;
            size -= read_bytes;
            ret += read_bytes;
        }
    }
    return ret;
}

static ssize_t write_all(int fd, const void* buffer, size_t size)
{
    ssize_t ret = 0;
    while(size) {
        ssize_t written = write(fd, buffer, size);
        if(written == -1 && errno != EINTR) {
            return -1;
        } else if(written != -1) {
            buffer = (const unsigned char*)buffer + written;
            size -= written;
            ret += written;
        }
    }
    return ret;
}

int msgsend(int port, struct message* msg)
{
    uint32_t signature = SIGNATURE;
    ssize_t ret = write_all(_socket, &signature, sizeof(signature));
    if(ret == -1) {
        perror("write()");
        return -1;
    }

    ret = write_all(_socket, msg, sizeof(struct message) + msg->len);
    if(ret == -1) {
        perror("write()");
        return -1;
    }
    return 0;
}

int msgrecv(int port, struct message* buffer, unsigned buffer_size, unsigned* outsize)
{
    if(buffer_size < sizeof(struct message)) {
        return -1;
    }

    uint32_t signature = 0xFFFFFFFF;
    ssize_t ret = read_all(_socket, &signature, sizeof(signature));
    if(ret == -1) {
        perror("read()");
        return -1;
    } else if(ret == 0) {
        printf("EOF\n");
        return -1;
    }
    assert(signature == SIGNATURE);

    ret = read_all(_socket, buffer, sizeof(struct message));
    if(ret == -1) {
        perror("read()");
        return -1;
    } else if(ret == 0) {
        printf("EOF\n");
        return -1;
    }
    
    buffer_size -= sizeof(struct message);

    if(buffer->len) {
        if(buffer_size < buffer->len) {
            return -1;
        }

        ret = read_all(_socket, buffer->data, buffer->len);
        if(ret == -1) {
            perror("read()");
            return -1;
        } else if(ret == 0) {
            printf("EOF\n");
            return -1;
        }
    }

    return 0;
}

static void serve_client(int sock, struct sockaddr_in* addr)
{
    _socket = sock;
    dispatch(-1);
}

static void start_server(int portnum)
{
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
        perror("socket()");
        exit(1);
    }

    struct sockaddr_in sai;
    sai.sin_family = AF_INET;
    sai.sin_port = htons(portnum);
    sai.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(sock, (struct sockaddr *)&sai, sizeof(sai)) < 0) {
        perror("bind()");
        exit(1);
    } else if(listen(sock, 1) < 0) {
        perror("listen()");
        exit(1);
    }

    while(1) {
        struct sockaddr_in clt_addr;
        socklen_t clt_addr_size = sizeof(clt_addr);
        int clt_sock = accept(sock, (struct sockaddr*)&clt_addr, &clt_addr_size);
        if(clt_sock < 0 && errno != EINTR) {
            perror("accept()");
            exit(EXIT_FAILURE);
        } else if(errno != EINTR) {
            serve_client(clt_sock, &clt_addr);
        }
        break;
    }
    close(sock);
}

static void start_client(const char* host, int portnum)
{
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
        perror("socket()");
        return;
    }

    struct hostent* he = gethostbyname(host);
    if(!he) {
        perror("gethostbyname()");
        return;
    }

    struct sockaddr_in sai;
    sai.sin_family = AF_INET;
    sai.sin_port = htons(portnum);
    sai.sin_addr = *((struct in_addr*)he->h_addr);

    if(connect(sock, (struct sockaddr*)&sai, sizeof(sai))) {
        perror("connect()");
        return;
    }

    _socket = sock;

    /* call rpc client wrappers */
    int ret0 = ret_int(-1, -1);
    printf("[CLT] ret_int() => %d\n", ret0);

    long long ret1 = ret_long(-1, -1);
    printf("[CLT] ret_long() => %lld\n", ret1);

    printf("[CLT] in_int(42)\n");
    in_int(-1, -1, 42);

    printf("[CLT] in_long(1337)\n");
    in_long(-1, -1, 1337);
    
    const char* str0 = "All your base are belong to us";
    printf("[CLT] in_string(\"%s\")\n", str0);
    in_string(-1, -1, str0);
    
    unsigned char blob0[4];
    *((uint32_t*)blob0) = 0xDEADBEEF;
    printf("[CLT] in_blob(0xDEADBEEF)\n");

    int out0 = -1;
    out_int(-1, -1, &out0);
    printf("[CLT] out_int() => %d\n", out0);

    long long out1 = -1;
    out_long(-1, -1, &out1);
    printf("[CLT] out_long() => %lld\n", out1);

    char str1[256];
    memset(str1, 0, sizeof(str1));
    out_string(-1, -1, str1, sizeof(str1));
    printf("[CLT] out_string() => %s\n", str1);

    unsigned char blob1[46];
    size_t blob1_size = sizeof(blob1);
    out_blob(-1, -1, blob1, &blob1_size);
    printf("[CLT] out_blob() => 0x");
    for(size_t i = 0; i < blob1_size; i++) {
        printf("%02X", blob1[i]);
    }
    printf(" (%zu bytes)\n", blob1_size);

    printf("[CLT] ow(\"%s\")\n", str0);
    ow(-1, -1, str0);

    close(sock);
}

static void print_usage(const char* program)
{
    printf("Usage: %s <mode> [options]\n"
           "Modes:\n"
           "\t-c\tClient mode\n"
           "\t-s\tServer mode\n"
           "Options:\n"
           "\t-h <address>\tHost to connect to when in client mode\n"
           "\t-p <port>\tPort to use\n",
           program);
}

int main(int argc, char** argv)
{
    opterr = 0;

    int c;
    int arg_clt = 0, arg_srv = 0;
    const char* arg_host = NULL, *arg_port = NULL;
    while((c = getopt(argc, argv, "csh:p:")) != -1) {
        switch(c) {
            case 'c': /* client mode */
                arg_clt = 1;
                break;
            case 's': /* server mode */
                arg_srv = 1;
                break;
            case 'h': /* host (client mode) */
                arg_host = optarg;
                break;
            case 'p':
                arg_port = optarg;
                break;
            case '?':
                if(optopt == 'h' || optopt == 'p') {
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                } else if(isprint(optopt)) {
                    fprintf(stderr, "Unknown option -%c.\n", optopt);
                } else {
                    fprintf(stderr, "Unknown option \\x%x.\n", optopt);
                }
                return 1;
        }
    }

    if(!arg_clt && !arg_srv) {
        fprintf(stderr, "Either -s or -c options must be specified.\n");
        print_usage(argv[0]);
        return 1;
    } else if(arg_clt && (!arg_host || !arg_port)) {
        fprintf(stderr, "Host/port not specified.\n");
        print_usage(argv[0]);
        return 1;
    } else if(arg_srv && !arg_port) {
        fprintf(stderr, "Port not specified.\n");
        print_usage(argv[0]);
    }


    if(arg_srv)
        start_server(atoi(arg_port));
    else
        start_client(arg_host, atoi(arg_port));

    return 0;
}






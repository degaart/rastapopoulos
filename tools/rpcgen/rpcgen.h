#pragma once

#define alloc(s) calloc(1, sizeof(struct s))
#define panic(...) __panic(__FILE__, __LINE__, __func__, __VA_ARGS__)

struct hc {
    FILE* h;
    FILE* c;
};

struct files {
    struct hc srv;
    struct hc comm;
    struct hc clt;
    const char* prefix;
};
extern struct files _files;


char* strupper(char* str);
char* stringify_type(const struct RPCType* type);
void __panic(const char* file, int line, const char* function, 
             const char* fmt, ...);
void generate_clientside(const struct RPCStatement* statements);
void generate_serverside(const struct RPCStatement* statements);
void generate_commonside(const struct RPCStatement* statements);



#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <assert.h>

#define READELF "i686-pc-elf-readelf"
#define CXXFILT "i686-pc-elf-c++filt"

struct symbol_t {
    unsigned long addr;
    char* name;
    unsigned size;
    struct symbol_t* next;
};

struct symtab_t {
    uint32_t addr;
    uint32_t size;
    uint32_t name_offset;
};

struct pipe_t {
    int read_end;
    int write_end;
};

struct pipe_t* pipe_create() {
    int fds[2];
    if(pipe(fds)) {
        perror("pipe()");
        exit(EXIT_FAILURE);
    }
    
    struct pipe_t* result = malloc(sizeof(struct pipe_t));
    result->write_end = fds[1];
    result->read_end = fds[0];
    return result;
}

pid_t exec_process(const char* const args[], int fd_in, int fd_out) {
    pid_t child = fork();
    if (child == 0) {
        int ret = -1;
        
        if(fd_in != STDIN_FILENO) {
            while (((ret = dup2(fd_in, STDIN_FILENO) == -1)) && (errno == EINTR));
            if(ret == -1) {
                perror("dup2()");
                exit(EXIT_FAILURE);
            }
            close(fd_in);
        }
        
        if(fd_out != STDOUT_FILENO) {
            while (((ret = dup2(fd_out, STDOUT_FILENO) == -1)) && (errno == EINTR));
            if(ret == -1) {
                perror("dup2()");
                exit(EXIT_FAILURE);
            }
            close(fd_out);
        }
        
        execvp(args[0], (char* const *)args);/* execv doesn't search PATH */
        perror("execv()");
        exit(EXIT_FAILURE);
        return -1;
    } else if(child == -1) {
        perror("fork()");
        return -1;
    } else {
        return child;
    }
}

struct symbol_t* read_symbol(char* buf) {
    /* Remove newline from buf */
    while(strlen(buf) && buf[strlen(buf)-1] == '\n') {
        buf[strlen(buf)-1] = '\0';
    }
    
    /* Split into fields, if field 4 == 'FUNC', output to cxxfilt */
    char* fields[12];
    memset(fields, 0, sizeof(fields));
    char* lasts = NULL;
    int nfields = 0;
    
    for(int i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        char* word;
        if(i == 0)
            word = strtok_r(buf, " ", &lasts);
        else if(i == 7) {
            fields[i] = lasts;
            nfields++;
            break;
        } else
            word = strtok_r(NULL, " ", &lasts);
        if(!word)
            break;
        fields[i] = word;
        nfields++;
    }
    
    if(nfields == 8 && !strcmp(fields[3], "FUNC")) {
        const char* addr = fields[1];
        const char* symbol = fields[7];
        unsigned size = atoi(fields[2]);
        
        struct symbol_t* result = malloc(sizeof(struct symbol_t));
        bzero(result, sizeof(*result));
        result->addr = strtoul(addr, NULL, 16);
        result->name = strdup(symbol);
        result->size = size;
        return result;
    } else {
        return NULL;
    }
}

struct symbol_t* read_symbols(const char* infile) {
    struct pipe_t* readelf_pipe = pipe_create();
    struct pipe_t* cxxfilt_pipe = pipe_create();
    
    const char* readelf_args[] = { READELF, "-W", "-s", infile, NULL };
    const char* cxxfilt_args[] = { CXXFILT, "-p", NULL };
    
    int infd = open(infile, O_RDONLY);
    if(infd == -1) {
        perror("open()");
        exit(EXIT_FAILURE);
    }
    
    pid_t readelf_pid = exec_process(readelf_args, infd, readelf_pipe->write_end);
    close(infd);
    infd = -1;
    close(readelf_pipe->write_end);
    readelf_pipe->write_end = -1;
    
    pid_t cxxfilt_pid = exec_process(cxxfilt_args, readelf_pipe->read_end, cxxfilt_pipe->write_end);
    close(readelf_pipe->read_end);
    readelf_pipe->read_end = -1;
    close(cxxfilt_pipe->write_end);
    cxxfilt_pipe->write_end = -1;
    
    FILE* inf = fdopen(cxxfilt_pipe->read_end, "rb");
    if(!inf) {
        perror("fdopen()");
        close(cxxfilt_pipe->read_end);
        kill(readelf_pid, SIGTERM);
        kill(cxxfilt_pid, SIGTERM);
        
        waitpid(readelf_pid, NULL, 0);
        waitpid(cxxfilt_pid, NULL, 0);
        exit(EXIT_FAILURE);
    }
    
    struct symbol_t* symbols = NULL;
    char buf[2048];
    while (1) {
skip_symbol:
        if(!fgets(buf, sizeof(buf), inf)) {
            if(feof(inf))
                break;
            else if(errno != EAGAIN && errno != EINTR) {
                perror("fgets()");
                fclose(inf);
                kill(readelf_pid, SIGTERM);
                kill(cxxfilt_pid, SIGTERM);
                
                waitpid(readelf_pid, NULL, 0);
                waitpid(cxxfilt_pid, NULL, 0);
                exit(EXIT_FAILURE);
            }
        } else {
            struct symbol_t* sym = read_symbol(buf);
            if(sym) {
                sym->next = symbols;
                symbols = sym;
            }
        }
    }
    fclose(inf);
    waitpid(readelf_pid, NULL, 0);
    waitpid(cxxfilt_pid, NULL, 0);
    
    return symbols;
}

static void serialize(const void* buffer, size_t size, FILE* f) {
    ssize_t ret = fwrite(buffer, size, 1, f);
    if (ret != 1) {
        perror("fwrite()");
        exit(EXIT_FAILURE);
    }
}

void serialize_symbols(struct symbol_t* symbols, const char* outfile) {
    /*
     Serialized format
        uint32_t        size
        symtab_t[]      table
        char[][]        name table
     */
    FILE* outf = fopen(outfile, "wb");
    
    uint32_t symbol_count = 0;
    for(struct symbol_t* sym = symbols; sym; sym = sym->next) {
        symbol_count++;
    }
    
    struct symtab_t entry;
    serialize(&symbol_count, sizeof(symbol_count), outf);
    uint32_t name_offset = sizeof(uint32_t) + (sizeof(struct symtab_t)*symbol_count);
    for(struct symbol_t* sym = symbols; sym; sym = sym->next) {
        entry.addr = sym->addr;
        entry.size = sym->size;
        entry.name_offset = name_offset;
        
        //printf("0x%08X\t%u\t%s\n", entry.addr, entry.size, sym->name);
        
        serialize(&entry, sizeof(entry), outf);
        name_offset += strlen(sym->name) + 1;
    }
    
    for(struct symbol_t* sym = symbols; sym; sym = sym->next) {
        serialize(sym->name, strlen(sym->name)+1, outf);
    }
    fclose(outf);
}

int main(int argc, const char * argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: gensyms <infile> <outfile>\n");
        exit(EXIT_FAILURE);
    }
    
    const char* infilename = argv[1];
    const char* outfilename = argv[2];
    signal(SIGCHLD, SIG_IGN);
    
    struct symbol_t* symbols = read_symbols(infilename);
    assert(symbols);
    for(struct symbol_t* sym = symbols; sym; sym = sym->next) {
        assert(sym->addr);
        assert(sym->name);
    }
    
    serialize_symbols(symbols, outfilename);
    return 0;
}



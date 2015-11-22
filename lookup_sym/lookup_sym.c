#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct symbol_t {
    uint32_t addr;
    uint32_t size;
    uint32_t name_offset;
};

struct symtab_t {
    char* data;
    struct symbol_t* symtab;
    unsigned count;
};

static struct symtab_t* load_syms(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if(!f) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    
    fseek(f, 0, SEEK_END);
    uint32_t size = (uint32_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    
    struct symtab_t* symtab = malloc(sizeof(struct symtab_t));
    symtab->data = malloc(size);
    if(fread(symtab->data, size, 1, f) != 1) {
        perror("fread");
        exit(EXIT_FAILURE);
    }
    fclose(f);
    symtab->count = *((uint32_t*)symtab->data);
    symtab->symtab = (struct symbol_t*) (((uint8_t*)symtab->data) + sizeof(uint32_t));
    return symtab;
}

int main(int argc, char** argv) {
    if(argc != 3) {
        fprintf(stderr, "Usage: lookup_sym <symfile> <address>\n");
        return 1;
    }
    
    uint32_t addr = strtoul(argv[2], NULL, 0);
    if(!addr) {
        fprintf(stderr, "Invalid value: %s\n", argv[2]);
        return 1;
    }
    
    struct symtab_t* syms = load_syms(argv[1]);
    for(unsigned i = 0; i < syms->count; i++) {
        if(addr>=syms->symtab[i].addr && (addr < syms->symtab[i].addr + syms->symtab[i].size)) {
            printf("%s\n", (const char*)(syms->data + syms->symtab[i].name_offset));
            return 0;
        }
    }
    printf("Not found\n");
    return 1;
}


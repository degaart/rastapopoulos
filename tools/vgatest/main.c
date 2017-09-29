#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "libx86emu-1.12/include/x86emu.h"

#define MEM_SIZE 0x100000

void logger(x86emu_t *emu, char *buf, unsigned size)
{
    printf("%*s", size, buf);
    fflush(stdout);
}

unsigned memio_handler(x86emu_t *emu, u32 addr, u32 *val, unsigned type)
{
    uint32_t bits = type & 0xFF;
    type &= ~0xFF;

    emu->mem->invalid = 0;
    void* memory = emu->_private;
    assert(addr < MEM_SIZE);

#define R(N,T) \
    case N: \
        *val = *((T*)(memory + addr)); \
        printf("read %s %s at 0x%04X => 0x%X\n", #N, #T, addr, *val); \
        break
#define X(N,T) \
    case N: \
        *val = *((T*)(memory + addr)); \
        printf("read instruction %s %s at 0x%04X => 0x%X\n", #N, #T, addr, *val); \
        break
#define W(N, T) \
    case N: \
        *((T*)(memory + addr)) = *val; \
        printf("write %s %s 0x%X at 0x%04X\n", #N, #T, *val, addr); \
        break


    switch(type) {
        case X86EMU_MEMIO_R:
            switch(bits) {
                R(X86EMU_MEMIO_8, uint8_t);
                R(X86EMU_MEMIO_16, uint16_t);
                R(X86EMU_MEMIO_32, uint32_t);
                R(X86EMU_MEMIO_8_NOPERM, uint8_t);
                default:
                    assert(!"Invalid code path");
            }
        case X86EMU_MEMIO_X:
            switch(bits) {
                X(X86EMU_MEMIO_8, uint8_t);
                X(X86EMU_MEMIO_16, uint16_t);
                X(X86EMU_MEMIO_32, uint32_t);
                X(X86EMU_MEMIO_8_NOPERM, uint8_t);
                default:
                    assert(!"Invalid code path");
            }
            break;
        case X86EMU_MEMIO_W:
            switch(bits) {
                W(X86EMU_MEMIO_8, uint8_t);
                W(X86EMU_MEMIO_16, uint16_t);
                W(X86EMU_MEMIO_32, uint32_t);
                W(X86EMU_MEMIO_8_NOPERM, uint8_t);
                default:
                    assert(!"Invalid code path");
            }
            break;
        case X86EMU_MEMIO_I:
            switch(bits) {
                case X86EMU_MEMIO_8:
                    printf("inb(%d)\n", addr);
                    *val = 0; 
                    break;
                case X86EMU_MEMIO_16:
                    printf("inw(%d)\n", addr);
                    *val = 0;
                    break;
                case X86EMU_MEMIO_32:
                    printf("inl(%d)\n", addr);
                    *val = 0;
                    break;
                default:
                    assert(!"Invalid code path");
            }
            break;
        case X86EMU_MEMIO_O:
            switch(bits) {
                case X86EMU_MEMIO_8:
                    printf("outb(%d, %d)\n", addr, *val);
                    break;
                case X86EMU_MEMIO_16:
                    printf("outw(%d, %d)\n", addr, *val);
                    break;
                case X86EMU_MEMIO_32:
                    printf("outl(%d, %d)\n", addr, *val);
                    break;
                default:
                    assert(!"Invalid code path");
            }
            break;
        default:
            assert(!"Invalid code path");
    }
    return 0;
#undef R
#undef X
#undef W
}

static unsigned read_program(x86emu_t* emu, uint32_t addr, const char* filename)
{
    FILE* f = fopen(filename, "rb");
    if(!f) {
        perror("open()");
        return 1;
    }

    unsigned char* ptr = emu->_private;
    ptr += addr;


    char buffer[512];
    unsigned result = 0;
    while(1) {
        ssize_t read_bytes = fread(buffer, 1, sizeof(buffer), f);
        if(read_bytes == 0) {
            if(feof(f)) {
                break;
            } else {
                perror("read");
                fclose(f);
                return -1;
            }
        }

        memcpy(ptr, buffer, read_bytes);
        ptr += read_bytes;
        result += read_bytes;
    }
    fclose(f);
    return result;
}

int main()
{
    x86emu_t* emu = x86emu_new(X86EMU_PERM_RWX, X86EMU_PERM_RWX);
    x86emu_set_log(emu, 512, logger);
    emu->log.trace = X86EMU_TRACE_DEFAULT;

    size_t memory_size = MEM_SIZE;
    unsigned char* memory = malloc(memory_size);
    emu->_private = memory;

    x86emu_set_memio_handler(emu, memio_handler);

    unsigned rret = read_program(emu, 0x7C00, "obj/program.bin");
    if(rret == -1) {
        return 1;
    }

    x86emu_set_seg_register(emu, emu->x86.R_CS_SEL, 0);
    emu->x86.R_IP = 0x7C00;

    emu->max_instr = 10;
    unsigned ret = x86emu_run(emu, X86EMU_RUN_LOOP|X86EMU_RUN_MAX_INSTR|X86EMU_RUN_NO_CODE);

#define X(T) if(ret & T) { printf(#T "\n"); }
    
    X(X86EMU_RUN_TIMEOUT);
    X(X86EMU_RUN_MAX_INSTR);
    X(X86EMU_RUN_NO_EXEC);
    X(X86EMU_RUN_NO_CODE);
    X(X86EMU_RUN_LOOP);

#undef X
   
    printf("ax: 0x%04X\n",
           emu->x86.R_AX);

    /*
     * crashes with SIGSEGV because the global variable
     * x86emu_optab is filled with NULL pointers (ops.c)
     */
    emu = x86emu_done(emu);
    assert(emu == NULL);
    return 0;
}


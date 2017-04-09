#include "../process.h"
#include "../debug.h"
#include "../util.h"
#include "../pmm.h"
#include "../vmm.h"
#include "../kernel.h"
#include "../string.h"
#include "../gdt.h"
#include "../context.h"
#include "../kmalloc.h"
#include "../registers.h"
#include "../timer.h"
#include "../io.h"
#include "../locks.h"
#include "../list.h"
#include "../syscall.h"
#include "../ipc.h"
#include "../scheduler.h"
#include "../initrd.h"
#include "../elf.h"

#define LoggerPort              1
#define LoggerMessageTrace      0
#define LoggerMessageTraceAck   1

/****************************************************************************
 * usermode syscall helpers
 ****************************************************************************/
static int USERFUNC port_open(int port_number)
{
    int result = syscall(SYSCALL_PORTOPEN,
                         port_number,
                         0,
                         0,
                         0,
                         0);
    return result;
}

static bool USERFUNC msgsend(int port, const struct message* msg)
{
    unsigned checksum = message_checksum(msg);
    assert(checksum == msg->checksum);

    unsigned result = syscall(SYSCALL_MSGSEND, 
                              port, 
                              (uint32_t)msg, 
                              0,
                              0,
                              0);
    return result != 0;
}

static unsigned USERFUNC msgrecv(int port, struct message* buffer, size_t buffer_size, size_t* outsize)
{
    /* Buffer validation */
    bzero(buffer, buffer_size);
    unsigned result = syscall(SYSCALL_MSGRECV,
                              port,
                              (uint32_t)buffer,
                              buffer_size,
                              (uint32_t)outsize,
                              0);

    unsigned checksum = message_checksum(buffer);
    assert(buffer->checksum == checksum);

    return result;
}

static bool USERFUNC msgpeek(int port)
{
    unsigned ret = syscall(SYSCALL_MSGPEEK,
                           port,
                           0,
                           0,
                           0,
                           0);
    return ret != 0;
}

static void USERFUNC yield()
{
    syscall(SYSCALL_YIELD, 0, 0, 0, 0, 0);
}

static int USERFUNC user_fork()
{
    int pid = syscall(SYSCALL_FORK, 0, 0, 0, 0, 0);
    return pid;
}

#define user_trace(ack_port, fmt, ...) \
    do {                                                \
        USERSTR(_fmt_, fmt);     \
        __user_trace(ack_port, _fmt_, ##__VA_ARGS__);             \
    } while(0)

static void USERFUNC __user_trace(int ack_port, const char* format, ...)
{
    va_list args;

    unsigned char buffer[128];
    struct message* msg = (struct message*)buffer;

    va_start(args, format);
    vsnprintf((char*)msg->data, sizeof(buffer) - sizeof(struct message), format, args);
    va_end(args);

    msg->sender = 0;
    msg->reply_port = ack_port;
    msg->code = LoggerMessageTrace;
    msg->len = strlen((const char*)msg->data) + 1;
    msg->checksum = message_checksum(msg);

    bool ret = msgsend(LoggerPort, msg);
    assert(ret);

    /* Wait ack */
    size_t outsize;
    unsigned recv_ret = msgrecv(ack_port, msg, sizeof(buffer), &outsize);
    assert(recv_ret == 0);

    assert(msg->checksum == message_checksum(msg));
    assert(msg->code == LoggerMessageTraceAck);
}

static void USERFUNC user_sleep(unsigned ms)
{
    syscall(SYSCALL_SLEEP,
            ms,
            0,
            0,
            0,
            0);
}

static void USERFUNC user_exit()
{
    syscall(SYSCALL_EXIT,
            0,
            0,
            0,
            0,
            0);
}

static void USERFUNC send_ack(int port, unsigned code, uint32_t result)
{
    unsigned char buffer[sizeof(struct message) + sizeof(uint32_t)] = {0};
    struct message* msg = (struct message*)buffer;
    msg->code = code;
    msg->len = sizeof(uint32_t);
    *((uint32_t*)msg->data) = result;
    msg->checksum = message_checksum(msg);

    bool ret = msgsend(port, msg);
    assert(ret);
}

static void USERFUNC setname(const char* new_name)
{
    syscall(SYSCALL_SETNAME, (uint32_t)new_name, 0, 0, 0, 0);
}

/****************************************************************************
 * test usermode programs
 ****************************************************************************/
static unsigned USERFUNC fibonacci(unsigned n)
{
    unsigned result;

    if (n == 0)
        result = 0;
    else if (n == 1)
        result = 1;
    else
        result = fibonacci(n - 1) + fibonacci(n - 2);

    return result;
} 

static unsigned USERFUNC is_prime(unsigned num)
{
    if(num == 1)
        return 1;

    for(unsigned i = 2; i < num; i++) {
        if(!(num % i))
            return 0;
    }

    return 1;
}

static void USERFUNC fibonacci_entry()
{
    USERSTR(proc_name, "fibonacci");
    setname(proc_name);

    int ack_port = port_open(-1);

    for(unsigned i = 0; i < 37; i++) {
        unsigned fib = fibonacci(i);
        user_trace(ack_port, "fib(%d): %d", i, fib);
    }

    user_trace(ack_port, "fib: Done");
    user_exit();
    invalid_code_path();
}

static void USERFUNC sleeper_entry()
{
    USERSTR(proc_name, "sleeper");
    setname(proc_name);

    int ack_port = port_open(-1);

    for(unsigned i = 0; i < 20; i++) {
        user_sleep(1000);
        user_trace(ack_port, "sleeper: %d", i);
    }

    user_trace(ack_port, "sleeper: Done");

    user_exit();
    invalid_code_path();
}

static void USERFUNC user_entry()
{
    USERSTR(proc_name, "primes");
    setname(proc_name);

    unsigned start_val = 5001000;

    /* Fork into other tasks */
    int pid = user_fork();
    if(!pid) {
        start_val = 5002000;
    } else {
        pid = user_fork();
        if(!pid) {
            start_val = 5003000;
        } else {
            pid = user_fork();
            if(!pid) {
                fibonacci_entry();
                goto exit;
            } else {
                pid = user_fork();
                if(!pid) {
                    sleeper_entry();
                    goto exit;
                }
            }
        }
    }

    int ack_port = port_open(-1);

    unsigned end_val = start_val + 500;
    for(unsigned i = start_val; i < end_val; i++) {
        if(is_prime(i)) {
            user_trace(ack_port, "prime: %d", i);
        }
    }

exit:
    user_trace(ack_port, "primes: Done");
    user_exit();
    invalid_code_path();
}

static void USERFUNC debug_outstr(const char* str)
{
    while(*str) {
        outb(DEBUG_PORT, *str);
        str++;
    }
}

static void USERFUNC logger_entry()
{
    USERSTR(proc_name, "logger");
    setname(proc_name);

    int ret = port_open(LoggerPort);
    if(ret < 0) {
        USERSTR(errmsg, "Failed to open logger port\n");
        debug_outstr(errmsg);
        while(1);
    }

    unsigned char buffer[512];
    struct message* msg = (struct message*)buffer;
    while(1) {
        size_t outsiz;
        unsigned ret = msgrecv(LoggerPort, 
                               msg, 
                               sizeof(buffer), 
                               &outsiz);
        if(ret != 0) {
            USERSTR(str, "msgrecv failed\n");
            debug_outstr(str);
            while(1);
        }

        if(msg->code == LoggerMessageTrace) {
            USERSTR(prefix, "[test_scheduler.c][logger_entry] ");
            debug_outstr(prefix);

            debug_outstr((const char*)(msg->data));

            USERSTR(suffix, "\n");
            debug_outstr(suffix);

            send_ack(msg->reply_port, LoggerMessageTraceAck, 0);
        } else {
            USERSTR(errstr, "Invalid message code");
            debug_outstr(errstr);
        }
    }
}

static void USERFUNC init_entry()
{
    int logger_pid = user_fork();
    if(!logger_pid) {
        logger_entry();
        invalid_code_path();
    }

    int userprog_pid = user_fork();
    if(!userprog_pid) {
        user_entry();
        invalid_code_path();
    }

    user_exit();
    invalid_code_path();
}

static void test_load_initrd()
{
    /*
     * Load ELF file
     */
    const struct initrd_file* init_file = initrd_get_file("init.elf");
    assert(init_file != NULL);
    const unsigned char* file_data = init_file->data;

    /*
     * Check ELF file headers
     */
    Elf32_Ehdr* ehdr = init_file->data;
    assert(ehdr->e_ident[EI_MAG0] == 0x7F);
    assert(ehdr->e_ident[EI_MAG1] == 'E');
    assert(ehdr->e_ident[EI_MAG2] == 'L');
    assert(ehdr->e_ident[EI_MAG3] == 'F');
    assert(ehdr->e_ident[EI_CLASS] == ELFCLASS32);
    assert(ehdr->e_ident[EI_DATA] == ELFDATA2LSB);
    assert(ehdr->e_type == ET_EXEC);
    assert(ehdr->e_machine == EM_386);
    assert(ehdr->e_version == EV_CURRENT);
    assert(ehdr->e_entry != NULL);

    /*
     * Load program segments
     */
    Elf32_Phdr* phdrs = (Elf32_Phdr*)(file_data + ehdr->e_phoff);
    assert(ehdr->e_phentsize == sizeof(Elf32_Phdr));
    for(int i = 0; i < ehdr->e_phnum; i++) {
        trace("Loading segment %d", i);

        Elf32_Phdr* phdr = phdrs + i;
        trace("\ttype: %p, offset: %p, vaddr: %p, filesz: %p, memsz: %p, flags: %p, align: %p",
              phdr->p_type,
              phdr->p_offset,
              phdr->p_vaddr,
              phdr->p_filesz,
              phdr->p_memsz,
              phdr->p_flags,
              phdr->p_align);

        if(phdr->p_type == PT_LOAD) {
            assert(phdr->p_memsz != 0);

            unsigned page_flags = VMM_PAGE_PRESENT | VMM_PAGE_USER;
            if(phdr->p_flags & PF_W)
                page_flags |= VMM_PAGE_WRITABLE;

            unsigned char* segment_start = phdr->p_vaddr;
            unsigned char* segment_end = segment_start + phdr->p_memsz;
            assert(segment_start >= (unsigned char*)USER_START);
            assert(segment_end <= (unsigned char*)USER_END);

            for(unsigned char* page = segment_start; page < segment_end; page += PAGE_SIZE) {
                vmm_map(page,
                        pmm_alloc(),
                        VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
            }

            bzero(segment_start, phdr->p_memsz);
            if(phdr->p_filesz) {
                memcpy(segment_start,
                       file_data + phdr->p_offset,
                       phdr->p_filesz);
            }

            for(unsigned char* page = segment_start; page < segment_end; page += PAGE_SIZE) {
                vmm_remap(page, page_flags);
            }
        }
    }

    trace("Starting execution at %p", ehdr->e_entry);
    scheduler_start((void(*)())ehdr->e_entry);

    reboot();
}

/****************************************************************************
 * Initialization
 ****************************************************************************/
void test_scheduler()
{
    trace("Testing scheduler");

#if 1
    test_load_initrd();
#else
    scheduler_start(init_entry);
#endif
}




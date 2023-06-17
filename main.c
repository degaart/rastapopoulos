#define PORT_COM1 0x3F8

static inline void outb(unsigned short port, unsigned char val)
{
    asm volatile(
            "outb %0, %1"
            :
            : "a"(val), "Nd"(port)
            : "memory");
}

static void serial_write_string(const char* s)
{
    while(*s) {
        outb(PORT_COM1, *s);
        s++;
    }
}

void kmain()
{
    serial_write_string("\nBackstreet's back all right\n");
}


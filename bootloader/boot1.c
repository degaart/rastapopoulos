#include "elf.h"
#include "multiboot.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

#define TRACE(...) trace(__FILE__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__)
#define DUMP(var) TRACE(#var ": %u", var)
#define DUMPX(var) TRACE(#var ": 0x%X", var);
#define PANIC(...) do { TRACE("*** PANIC ***"); TRACE(__VA_ARGS__); while(1); } while(0)
#define ASSERT(cond) if(!(cond)) { PANIC("Assertion failed: " #cond); }

#define COLOR_BLACK         0x00
#define COLOR_BLUE          0x01
#define COLOR_GREEN         0x02
#define COLOR_CYAN          0x03
#define COLOR_RED           0x04
#define COLOR_MAGENTA       0x05
#define COLOR_BROWN         0x06
#define COLOR_LIGHTGRAY     0x07
#define COLOR_DARKGRAY      0x08
#define COLOR_LIGHBLUE      0x09
#define COLOR_LIGHTGREEN    0x0A
#define COLOR_LIGHCYAN      0x0B
#define COLOR_LIGHRED       0x0C
#define COLOR_LIGHTMAGENTA  0x0D
#define COLOR_YELLOW        0x0E
#define COLOR_WHITE         0x0F

struct rmode_regs {
    uint16_t ax;
    uint16_t bx;
    uint16_t cx;
    uint16_t dx;
    uint16_t si;
    uint16_t di;
    uint16_t bp;
    uint16_t es;
    uint16_t flags;
} __attribute__((packed));

struct bpb {
    char jump[3];
    char oem_name[8];
    uint16_t bytes_per_sect;
    uint8_t sect_per_clus;
    uint16_t rsvd_sect_count;
    uint8_t fat_count;
    uint16_t root_ent_count;
    uint16_t total_sectors16;
    uint8_t media;
    uint16_t fat_size16;
    uint16_t sect_per_track;
    uint16_t num_heads;
    uint32_t hidden_sect_count;
    uint32_t total_sectors32;
    uint8_t drive_num;
    uint8_t reserved1;
    uint8_t bootsig;
    uint32_t volid;
    char label[11];
    char fstype[8];
} __attribute__((packed));

extern void __attribute__((stdcall)) writechar32(uint32_t, uint32_t);
extern void __attribute__((stdcall)) int13(struct rmode_regs*);
extern unsigned char __heap__;
static unsigned char* _heap_start = &__heap__;

#define SECTOR_CACHE_SIZE 8
#define INVALID_LBA 0xFFFFFFFF
struct sector_cache_entry {
    uint32_t lba;           /* 0xFFFFFFFF: unused entry */
    void* buffer;
};
struct sector_cache_entry sector_cache[SECTOR_CACHE_SIZE];

#define FILENAME "KERNEL  ELF"
#define KERNEL_CRC32 0x7A947892

struct dir_entry {
    char name[11];
    uint8_t attrs;
    unsigned char ignore[14];
    uint16_t first_cluster;
    uint32_t size;
} __attribute__((packed));

#define IDT_PRESENT        (1 << 7)
#define IDT_DPL0           (0)
#define IDT_DPL1           (1 << 5)
#define IDT_DPL2           (2 << 5)
#define IDT_DPL3           (3 << 5)
#define IDT_TASK_GATE      (5)
#define IDT_TSS_32_AVL     (9)
#define IDT_TSS_32_BUSY    (11)
#define IDT_INT_GATE_32    (14)
#define IDT_TRAP_GATE_32   (15)

struct idt_entry {
    uint16_t offset_lowerbits;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_higherbits;
} __attribute__((packed));
static struct idt_entry idt_entries[18];

struct idt_ptr {
    uint16_t limit;
    struct idt_entry* base;
} __attribute__((packed));
static struct idt_ptr idtr __attribute__((aligned(8)));

static void trace(const char* file, int line, const char* fn, const char* fmt, ...);
void isr0_stub(void);
void isr1_stub(void);
void isr2_stub(void);
void isr3_stub(void);
void isr4_stub(void);
void isr5_stub(void);
void isr6_stub(void);
void isr7_stub(void);
void isr8_stub(void);
void isr9_stub(void);
void isr10_stub(void);
void isr11_stub(void);
void isr12_stub(void);
void isr13_stub(void);
void isr14_stub(void);
void isr15_stub(void);
void isr16_stub(void);
void isr17_stub(void);
void isr18_stub(void);

#define CRC32_INITIAL 0xFFFFFFFF

/*
 * WARNING: The final crc value must be negated (1's complement)
 * i.e. final_crc = ~crc;
 */
static uint32_t crc32(uint32_t state, const void* buffer, size_t size)
{
    uint32_t r = state;
    const unsigned char* data = buffer;
    while(size--) {
        r ^= *data++;

        for(int i = 0; i < 8; i++) {
            uint32_t t = ~((r&1) - 1);
            r = (r>>1) ^ (0xEDB88320 & t);
        }
    }

    return r;
}

static size_t strlen(const char* s)
{
    size_t ret = 0;
    while(*(s++))
        ret++;
    return ret;
}

static void* memcpy(void* restrict dst, const void* restrict src, size_t len)
{
    unsigned char* d = dst;
    const unsigned char* s = src;
    while(len--) {
        *d++ = *s++;
    }
    return dst;
}

static int memcmp(const void* ptr0, const void* ptr1, size_t len)
{
    const char* p0 = ptr0;
    const char* p1 = ptr1;
    while(len) {
        if(*p0 != *p1) {
            return *p0 - *p1;
        }
        p0++;
        p1++;
        len--;
    }
    return 0;
}

static void* memset(void* dst, int ch, size_t len)
{
    unsigned char* ptr = dst;
    while(len--) {
        *ptr++ = ch;
    }
    return dst;
}

static void itox(char* buffer, size_t size, unsigned value)
{
    if(!value) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    char tmp[12];
    char* p = tmp;
    while(value) {
        int digit = value % 16;
        *(p++) = digit + (digit < 10 ? '0' : 'A' - 10);
        value /= 16;
    }

    for(--p; p>=tmp && size > 1; size--) {
        *(buffer++) = *(p--);
    }
    *buffer = '\0';
}

static void itoa(char* buffer, size_t size, unsigned value)
{
    if(value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    char tmp[9];
    char* p = tmp;
    while(value) {
        *(p++) = (value % 10) + '0';
        value /= 10;
    }

    for(--p; p>=tmp && size > 1; size--) {
        *(buffer++) = *(p--);
    }

    *buffer = '\0';
}

static void writestring32(const char* s)
{
    while(*s) {
        writechar32(*s, COLOR_LIGHTGRAY);
        s++;
    }
}

static inline void outb(uint16_t port, uint8_t val)
{
    asm volatile(
            "outb %1, %0"
            :
            : "a"(val), "Nd"(port)
            : "memory");
}

#define PORT_COM1 0x3F8

void serial_write_char(char ch)
{
    outb(PORT_COM1, ch);
}

void serial_write_string(const char* s)
{
    while(*s) {
        serial_write_char(*s);
        s++;
    }
}

void trace_init()
{
    serial_write_char('\n');
}

static void trace(const char* file, int line, const char* fn, const char* fmt, ...)
{
    writechar32('[', COLOR_LIGHTGRAY);
    serial_write_char('[');
    writestring32(file);
    serial_write_string(file);
    writechar32(':', COLOR_LIGHTGRAY);
    serial_write_char(':');

    writestring32(fn);
    serial_write_string(fn);
    writechar32(':', COLOR_LIGHTGRAY);
    serial_write_char(':');
    
    char line_buffer[16];
    itoa(line_buffer, sizeof(line_buffer), line);
    writestring32(line_buffer);
    serial_write_string(line_buffer);
    writestring32("] ");
    serial_write_string("] ");

    va_list args;
    va_start(args, fmt);
    while(*fmt) {
        switch(*fmt) {
            case '%':
                switch(*(fmt+1)) {
                    case 's':
                    {
                        const char* s = va_arg(args, const char*);
                        writestring32(s);
                        serial_write_string(s);
                        fmt++;
                        break;
                    }
                    case 'u':
                    case 'd':
                    {
                        unsigned value = va_arg(args, unsigned);
                        char buffer[16];
                        itoa(buffer, sizeof(buffer), value);
                        writestring32(buffer);
                        serial_write_string(buffer);
                        fmt++;
                        break;
                    }
                    case 'x':
                    case 'X':
                    {
                        unsigned value = va_arg(args, unsigned);
                        char buffer[16];
                        itox(buffer, sizeof(buffer), value);
                        writestring32(buffer);
                        serial_write_string(buffer);
                        fmt++;
                        break;
                    }
                    case 'p':
                    case 'P':
                    {
                        unsigned value = va_arg(args, unsigned);
                        char buffer[16];
                        itox(buffer, sizeof(buffer), value);
                        int pad = 8 - strlen(buffer);
                        writestring32("0x");
                        serial_write_string("0x");
                        for(int i = 0; i < pad; i++) {
                            writechar32('0', COLOR_LIGHTGRAY);
                            serial_write_char('0');
                        }
                        writestring32(buffer);
                        serial_write_string(buffer);
                        fmt++;
                        break;
                    }
                    case '%':
                    {
                        fmt++;
                        writechar32('%', COLOR_LIGHTGRAY);
                        serial_write_char('%');
                        break;
                    }
                }
                break;
            case '\0':
                break;
            default:
                writechar32(*fmt, COLOR_LIGHTGRAY);
                serial_write_char(*fmt);
                break;
        }
        fmt++;
    }
    writechar32('\r', COLOR_LIGHTGRAY);
    serial_write_char('\r');
    writechar32('\n', COLOR_LIGHTGRAY);
    serial_write_char('\n');
}

static unsigned bios_read_sector(void* buffer, size_t len, unsigned drive,
        unsigned c, unsigned h, unsigned s)
{
    ASSERT((uintptr_t)buffer <= 0xFFFF);

    struct rmode_regs regs = {0};
    regs.ax = 0x0201;
    regs.cx = (s & 0xFF)|((c & 0xFF) << 8);
    regs.dx = (drive & 0xFF)|((h & 0xFF) << 8);
    regs.es = 0;
    ASSERT((uintptr_t)buffer < 0xFFFF);
    regs.bx = (uint32_t)(uintptr_t)buffer;
    int13(&regs);
    asm volatile("lidt %0" :: "m"(idtr));
    return (regs.ax >> 8) & 0xFF;
}

static void bios_reset_disk()
{
    struct rmode_regs regs = {0};
    regs.ax = 0;
    int13(&regs);
    asm volatile("lidt %0" :: "m"(idtr));
}

static bool load_sector_chs(
        void* buffer, size_t len, unsigned drive,
        unsigned c, unsigned h, unsigned s)
{
    struct rmode_regs regs = {0};
    for(unsigned i = 0; i < 9; i++) {
        unsigned ret = bios_read_sector(buffer, len, drive, c, h, s);
        if(!ret) {
            return true;
        }
        TRACE("Disk read failed with status %d", ret);
        bios_reset_disk();
    }
    PANIC("I/O error reading C %u, H %u, S %u", c, h, s);
    return false;
}

static bool load_sector(const struct bpb* bpb, void* buffer, size_t len, uint32_t lba)
{
    ASSERT(lba < bpb->total_sectors16);
    uint32_t tmp = lba / bpb->sect_per_track;
    uint32_t sect = (lba % bpb->sect_per_track) + 1;
    uint32_t head = tmp % bpb->num_heads;
    uint32_t cyl = tmp / bpb->num_heads;
    return load_sector_chs(buffer, len, bpb->drive_num, cyl, head, sect);
}

static void* malloc(size_t len)
{
    unsigned char* result = (unsigned char*)((((uintptr_t)_heap_start + 15) / 16) * 16);
    TRACE("Allocated %d bytes at 0x%X", len, result);
    ASSERT((uintptr_t)result < 0x0007FFFF);
    _heap_start = result + len;
    return result;
}

static void sector_cache_init(const struct bpb* bpb)
{
    for(size_t i = 0; i < SECTOR_CACHE_SIZE; i++) {
        sector_cache[i].lba = INVALID_LBA;
        sector_cache[i].buffer = malloc(bpb->bytes_per_sect);

        /* 
         * Check the buffer does not cross 64k boundaries
         * If it does, we just realloc and it will word
         */
        uintptr_t buffer = (uintptr_t)sector_cache[i].buffer;
        if(buffer / 65536 != (buffer + bpb->bytes_per_sect - 1) / 65536) {
            sector_cache[i].buffer = malloc(bpb->bytes_per_sect);
        }
    }

    for(size_t i = 0; i < SECTOR_CACHE_SIZE; i++) {
        TRACE("sector_cache[%d].buffer: %p", i, sector_cache[i].buffer);
    }
}

static void* cached_load_sector(const struct bpb* bpb, uint32_t lba)
{
    /* check if given sector is in sector cache */
    unsigned sector = 0;
    size_t entry_index = SIZE_MAX;
    for(size_t i = 0; i < SECTOR_CACHE_SIZE; i++) {
        if(sector_cache[i].lba == lba) {
            entry_index = i;
            break;
        }
    }

    if(entry_index == SIZE_MAX) {
        /* We will take the buffer of last element */
        void* buffer = sector_cache[SECTOR_CACHE_SIZE-1].buffer;

        /* shift sectors to back of LRU queue */
        for(size_t i = SECTOR_CACHE_SIZE - 1; i > 0; i--) {
            sector_cache[i] = sector_cache[i-1];
        }
        sector_cache[0].lba = lba;
        sector_cache[0].buffer = buffer;

        if(!load_sector(bpb, sector_cache[0].buffer, bpb->bytes_per_sect, lba)) {
            PANIC("Failed to load sector 0x%X", lba);
        }
        entry_index = 0;
    } else if(entry_index != 0) {
        /* Move to front */
        struct sector_cache_entry entry = sector_cache[entry_index];
        for(size_t i = 0; i < entry_index; i++) {
            sector_cache[i+1] = sector_cache[i];
        }
        sector_cache[0] = entry;
        entry_index = 0;
    }

    return sector_cache[entry_index].buffer;
}

static void* load_cluster(const struct bpb* bpb, uint32_t cluster, unsigned sector_num)
{
    ASSERT(sector_num < bpb->sect_per_clus);
    unsigned root_dir_sectors =
        ((bpb->root_ent_count * 32) +
         (bpb->bytes_per_sect - 1)) /
        bpb->bytes_per_sect;
    unsigned first_data_sector =
        bpb->rsvd_sect_count +
        (bpb->fat_count * bpb->fat_size16) +
        root_dir_sectors;
    unsigned sector =
        ((cluster - 2) * bpb->sect_per_clus) +
        first_data_sector +
        sector_num;
    return cached_load_sector(bpb, sector);
}

static uint32_t next_cluster(const struct bpb* bpb, uint32_t cluster)
{
    unsigned fat_offset = cluster + (cluster / 2);
    unsigned fat_sector = bpb->rsvd_sect_count + (fat_offset / bpb->bytes_per_sect);
    unsigned entry_offset = fat_offset % bpb->bytes_per_sect;
    const void* buffer = cached_load_sector(bpb, fat_sector);
    uint16_t value = *(uint16_t*)((const unsigned char*)buffer + entry_offset);
    if(cluster & 1) {
        value = value >> 4;
    } else {
        value = value & 0xFFF;
    }
    return value;
}

void read_file(const struct bpb* bpb, const struct dir_entry* dirent,
        void* dest_buffer, size_t len, unsigned offset)
{
    ASSERT(len <= bpb->bytes_per_sect);
    ASSERT(offset < dirent->size);

    unsigned bytes_per_cluster = bpb->sect_per_clus * bpb->bytes_per_sect;
    unsigned cluster_index = offset / bytes_per_cluster;
    unsigned sector = (offset % bytes_per_cluster) / bpb->bytes_per_sect;
    unsigned sector_offset = offset % bpb->bytes_per_sect;

    unsigned cluster = dirent->first_cluster;
    for(unsigned i = 0; i < cluster_index; i++) {
        cluster = next_cluster(bpb, cluster);
    }
    ASSERT(cluster < bpb->total_sectors16 / bpb->sect_per_clus);
    ASSERT(sector < bpb->sect_per_clus);
    ASSERT(sector_offset < bpb->bytes_per_sect);

    /* Check for sector-straddling */
    if(bpb->bytes_per_sect - sector_offset < len) {
        const unsigned char* buffer = load_cluster(bpb, cluster, sector);
        memcpy(dest_buffer, buffer + sector_offset, bpb->bytes_per_sect - sector_offset);
        if(sector + 1 < bpb->sect_per_clus) {
            buffer = load_cluster(bpb, cluster, sector + 1);
            memcpy(dest_buffer + bpb->bytes_per_sect - sector_offset,
                buffer,
                len - bpb->bytes_per_sect + sector_offset);
        } else {
            buffer = load_cluster(bpb, cluster + 1, 0);
            memcpy(dest_buffer + bpb->bytes_per_sect - sector_offset,
                buffer,
                len - bpb->bytes_per_sect + sector_offset);
        }
    } else {
        const unsigned char* buffer = load_cluster(bpb, cluster, sector);
        memcpy(dest_buffer, buffer + sector_offset, len);
    }
}

void start()
{
#define SETSTUB(idx, handler) \
    do { \
        idt_entries[idx].offset_lowerbits = ((uintptr_t)handler & 0xFFFF); \
        idt_entries[idx].offset_higherbits = ((uintptr_t)handler >> 16) & 0xFFFF; \
    } while(0)

    /* Setup an IDT for debug purposes */
    for(size_t i = 0; i < sizeof(idt_entries)/sizeof(idt_entries[0]); i++) {
        //idt_entries[i].offset_lowerbits = ((uintptr_t)isr_stub32 & 0xFFFF);
        //idt_entries[i].offset_higherbits = ((uintptr_t)isr_stub32 >> 16) & 0xFFFF;
        idt_entries[i].selector = 0x08; /* kernel code segment */
        idt_entries[i].zero = 0;
        idt_entries[i].type_attr = IDT_PRESENT|IDT_DPL0|IDT_INT_GATE_32;
    }
    SETSTUB(0, isr0_stub);
    SETSTUB(1, isr1_stub);
    SETSTUB(2, isr2_stub);
    SETSTUB(3, isr3_stub);
    SETSTUB(4, isr4_stub);
    SETSTUB(5, isr5_stub);
    SETSTUB(6, isr6_stub);
    SETSTUB(7, isr7_stub);
    SETSTUB(8, isr8_stub);
    SETSTUB(9, isr9_stub);
    SETSTUB(10, isr10_stub);
    SETSTUB(11, isr11_stub);
    SETSTUB(12, isr12_stub);
    SETSTUB(13, isr13_stub);
    SETSTUB(14, isr14_stub);
    SETSTUB(15, isr15_stub);
    SETSTUB(16, isr16_stub);
    SETSTUB(17, isr17_stub);
    SETSTUB(18, isr18_stub);

    idtr.limit = sizeof(idt_entries) - 1;
    idtr.base = idt_entries;
    asm volatile("lidt %0" :: "m"(idtr));

    /* Bios parameter block */
    const struct bpb* bpb = (const struct bpb*)0x7C00;
    ASSERT(bpb->drive_num == 0);
    ASSERT(bpb->bytes_per_sect == 512);
    ASSERT(bpb->sect_per_clus == 1);
    ASSERT(bpb->rsvd_sect_count == 1);
    ASSERT(bpb->fat_count == 2);
    ASSERT(bpb->root_ent_count == 224);
    ASSERT(bpb->fat_size16 == 9);
    ASSERT(bpb->hidden_sect_count == 0);
    DUMP(bpb->total_sectors16);
    ASSERT(bpb->total_sectors16 == 2880);
    ASSERT(bpb->bytes_per_sect >= sizeof(struct Elf32_Ehdr));

    /* initialize sector cache */
    sector_cache_init(bpb);

    unsigned char* buffer = cached_load_sector(bpb, 0);
    ASSERT(buffer[0] == 0xEB);
    ASSERT(buffer[1] == 0x3C);
    ASSERT(buffer[2] == 0x90);
    ASSERT(buffer[510] == 0x55);
    ASSERT(buffer[511] == 0xAA);

    /*
     * Disk structure
     *  BPB                 rsvd_sect_count sectors
     *  FAT1                fat_size16 sectors
     *  FAT2                fat_size16 sectors
     *  Root Directory      root_dir_sects sectors
     *  Data area
     */

    /* Find file in root dir */
    ASSERT(sizeof(struct dir_entry) == 32);
    unsigned root_dir_sectors = ((bpb->root_ent_count * 32) + (bpb->bytes_per_sect - 1)) / bpb->bytes_per_sect;
    unsigned entries_per_sector = bpb->bytes_per_sect / 32;
    TRACE("Root dir starts at sector %d", bpb->rsvd_sect_count + (bpb->fat_count * bpb->fat_size16));
    struct dir_entry kernel_dir_entry = {0};
    for(unsigned cur_sector = bpb->rsvd_sect_count + (bpb->fat_count * bpb->fat_size16);; cur_sector++) {
        struct dir_entry* entries = cached_load_sector(bpb, cur_sector);
        for(unsigned i = 0; i < entries_per_sector; i++) {
            if(entries[i].name[0] == '\0') {
                PANIC("File not found: %s (ended at %d)", FILENAME, i);
            } else if(!memcmp(entries[i].name, FILENAME, 11)) {
                memcpy(&kernel_dir_entry, &entries[i], sizeof(kernel_dir_entry));
                break;
            }
        }
        if(kernel_dir_entry.name[0])
            break;
    }
    TRACE("Kernel image found at cluster 0x%X (%d bytes)", kernel_dir_entry.first_cluster, kernel_dir_entry.size);

    /*
     * Elf structure
     *
     *  ELF header
     *  Program Headers
     *  Sections
     *  Section Headers
     */

    /*
     * Read elf header
     * We have to copy the buffer as it may be reused in subsequent read
     * operations
     */
    struct Elf32_Ehdr elf_hdr;
    read_file(bpb, &kernel_dir_entry, &elf_hdr, sizeof(elf_hdr), 0);
    ASSERT(elf_hdr.e_ident[0] == ELFMAG0);
    ASSERT(elf_hdr.e_ident[1] == ELFMAG1);
    ASSERT(elf_hdr.e_ident[2] == ELFMAG2);
    ASSERT(elf_hdr.e_ident[3] == ELFMAG3);

    /* Read program headers */
    for(size_t i = 0; i < elf_hdr.e_phnum; i++) {
        unsigned offset = elf_hdr.e_ehsize + (elf_hdr.e_phentsize * i);
        struct Elf32_Phdr phdr;
        read_file(bpb, &kernel_dir_entry, &phdr, sizeof(phdr), offset);
        if(phdr.p_type == PT_LOAD) {
            TRACE("p_offset: 0x%X, "
                  "p_vaddr: 0x%X, "
                  "p_paddr: 0x%X, "
                  "p_filesz: %u, "
                  "p_memsz: %u, "
                  "p_align: %u",
                  phdr.p_offset,
                  phdr.p_vaddr,
                  phdr.p_paddr,
                  phdr.p_filesz,
                  phdr.p_memsz,
                  phdr.p_align);

            /* Load p_filesz from offset p_offset into p_vaddr */
            unsigned remaining = phdr.p_filesz;
            unsigned char* dst = (unsigned char*)phdr.p_vaddr;
            unsigned offset = phdr.p_offset;
            while(remaining) {
                unsigned len = remaining > bpb->bytes_per_sect ? bpb->bytes_per_sect : remaining;
                read_file(bpb, &kernel_dir_entry, dst, len, offset);
                remaining -= len;
                offset += len;
                dst += len;
            }

            /* Pad with zeroes */
            remaining = phdr.p_memsz - phdr.p_filesz;
            if(remaining) {
                TRACE("Padding %d bytes with zeroes at %p", remaining, dst);
                memset(dst, 0, remaining);
            }
        }
    }

    /* Jump to kernel */
    asm volatile(
              "jmp %0"
            :
            : "r"(elf_hdr.e_entry), "a"(MULTIBOOT_BOOTLOADER_MAGIC)
            : "memory");
    TRACE("SYSTEM HALTED");
    while(1);
}


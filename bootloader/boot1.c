#include <debug.h>
#include <elf.h>
#include <multiboot.h>
#include <serial.h>
#include <string.h>
#include <format.h>
#include <util.h>
#include <vga.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

extern void __attribute__((stdcall)) int13(struct rmode_regs*);
extern unsigned char __heap__;
unsigned char* _heap_start = &__heap__;
extern uint32_t isr_stub_table[];

#define SECTOR_CACHE_SIZE 8
#define INVALID_LBA       0xFFFFFFFF
struct sector_cache_entry {
    uint32_t lba; /* 0xFFFFFFFF: unused entry */
    void* buffer;
};
struct sector_cache_entry sector_cache[SECTOR_CACHE_SIZE];

#define FILENAME     "KERNEL  ELF"
#define KERNEL_CRC32 0x7A947892

struct dir_entry {
    char name[11];
    uint8_t attrs;
    unsigned char ignore[14];
    uint16_t first_cluster;
    uint32_t size;
} __attribute__((packed));

#define IDT_PRESENT      (1 << 7)
#define IDT_DPL0         (0)
#define IDT_DPL1         (1 << 5)
#define IDT_DPL2         (2 << 5)
#define IDT_DPL3         (3 << 5)
#define IDT_TASK_GATE    (5)
#define IDT_TSS_32_AVL   (9)
#define IDT_TSS_32_BUSY  (11)
#define IDT_INT_GATE_32  (14)
#define IDT_TRAP_GATE_32 (15)

struct idt_entry {
    uint16_t offset_lowerbits;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_higherbits;
} __attribute__((packed));
static struct idt_entry idt_entries[33];

struct idt_ptr {
    uint16_t limit;
    struct idt_entry* base;
} __attribute__((packed));
static struct idt_ptr idtr __attribute__((aligned(8)));

/*
    Note: ctx must point to a buffer of size STB_SPRINTF_MIN
*/
static char* debug_write(const char* buf, void* ctx, int len)
{
    while(len--) {
        serial_write_char(*buf);
        vga_write_char(*buf, COLOR_LIGHTGRAY);
        buf++;
    }
    return ctx;
}


void panic(const char* file, int line, const char* fn, const char* fmt, ...)
{
    trace(file, line, fn, "*** BOOTLOADER PANIC ***");

    va_list args;
    char buffer[64];
    va_start(args, fmt);
    formatv(debug_write, buffer, buffer, fmt, args);
    va_end(args);
    HALT();
}

void trace_init()
{
    serial_write_char('\n');
}

void trace(const char* file, int line, const char* fn, const char* fmt, ...)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "[%s:%s:%d] ", basename(file), fn, line);
    serial_write_string(buf);
    vga_write_string(buf, COLOR_LIGHTGRAY);

    va_list args;
    char buffer[64];
    va_start(args, fmt);
    formatv(debug_write, buffer, buffer, fmt, args);
    va_end(args);

    serial_write_string("\n");
    vga_write_string("\n", COLOR_LIGHTGRAY);
}

static unsigned bios_read_sector(void* buffer, size_t len, unsigned drive,
                                 unsigned c, unsigned h, unsigned s)
{
    ASSERT((uintptr_t)buffer <= 0xFFFF);

    struct rmode_regs regs = {0};
    regs.ax = 0x0201;
    regs.cx = (s & 0xFF) | ((c & 0xFF) << 8);
    regs.dx = (drive & 0xFF) | ((h & 0xFF) << 8);
    regs.es = 0;
    ASSERT((uintptr_t)buffer < 0xFFFF);
    regs.bx = (uint32_t)(uintptr_t)buffer;
    int13(&regs);
    asm volatile("lidt %0" ::"m"(idtr));
    return (regs.ax >> 8) & 0xFF;
}

static void bios_reset_disk()
{
    struct rmode_regs regs = {0};
    regs.ax = 0;
    int13(&regs);
    asm volatile("lidt %0" ::"m"(idtr));
}

static bool load_sector_chs(void* buffer, size_t len, unsigned drive,
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

static bool load_sector(const struct bpb* bpb, void* buffer, size_t len,
                        uint32_t lba)
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
    unsigned char* result =
        (unsigned char*)((((uintptr_t)_heap_start + 15) / 16) * 16);
    TRACE("Allocated %zd bytes at %p", len, result);
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
        TRACE("sector_cache[%zd].buffer: %p", i, sector_cache[i].buffer);
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
        void* buffer = sector_cache[SECTOR_CACHE_SIZE - 1].buffer;

        /* shift sectors to back of LRU queue */
        for(size_t i = SECTOR_CACHE_SIZE - 1; i > 0; i--) {
            sector_cache[i] = sector_cache[i - 1];
        }
        sector_cache[0].lba = lba;
        sector_cache[0].buffer = buffer;

        if(!load_sector(bpb, sector_cache[0].buffer, bpb->bytes_per_sect,
                        lba)) {
            PANIC("Failed to load sector 0x%lX", lba);
        }
        entry_index = 0;
    } else if(entry_index != 0) {
        /* Move to front */
        struct sector_cache_entry entry = sector_cache[entry_index];
        for(size_t i = 0; i < entry_index; i++) {
            sector_cache[i + 1] = sector_cache[i];
        }
        sector_cache[0] = entry;
        entry_index = 0;
    }

    return sector_cache[entry_index].buffer;
}

static void* load_cluster(const struct bpb* bpb, uint32_t cluster,
                          unsigned sector_num)
{
    ASSERT(sector_num < bpb->sect_per_clus);
    unsigned root_dir_sectors =
        ((bpb->root_ent_count * 32) + (bpb->bytes_per_sect - 1)) /
        bpb->bytes_per_sect;
    unsigned first_data_sector = bpb->rsvd_sect_count +
                                 (bpb->fat_count * bpb->fat_size16) +
                                 root_dir_sectors;
    unsigned sector =
        ((cluster - 2) * bpb->sect_per_clus) + first_data_sector + sector_num;
    return cached_load_sector(bpb, sector);
}

static uint32_t next_cluster(const struct bpb* bpb, uint32_t cluster)
{
    unsigned fat_offset = cluster + (cluster / 2);
    unsigned fat_sector =
        bpb->rsvd_sect_count + (fat_offset / bpb->bytes_per_sect);
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
        memcpy(dest_buffer, buffer + sector_offset,
               bpb->bytes_per_sect - sector_offset);
        if(sector + 1 < bpb->sect_per_clus) {
            buffer = load_cluster(bpb, cluster, sector + 1);
            memcpy(dest_buffer + bpb->bytes_per_sect - sector_offset, buffer,
                   len - bpb->bytes_per_sect + sector_offset);
        } else {
            buffer = load_cluster(bpb, cluster + 1, 0);
            memcpy(dest_buffer + bpb->bytes_per_sect - sector_offset, buffer,
                   len - bpb->bytes_per_sect + sector_offset);
        }
    } else {
        const unsigned char* buffer = load_cluster(bpb, cluster, sector);
        memcpy(dest_buffer, buffer + sector_offset, len);
    }
}

static void setup_idt()
{
    TRACE("Setting up IDT");
    /* Setup an IDT for debug purposes */
    for(size_t i = 0; i < sizeof(idt_entries) / sizeof(idt_entries[0]); i++) {
        idt_entries[i].offset_lowerbits = (isr_stub_table[i] & 0xFFFF);
        idt_entries[i].offset_higherbits = (isr_stub_table[i] >> 16) & 0xFFFF;
        idt_entries[i].selector = 0x08; /* kernel code segment */
        idt_entries[i].zero = 0;
        idt_entries[i].type_attr = IDT_PRESENT | IDT_DPL0 | IDT_INT_GATE_32;
    }
    idtr.limit = sizeof(idt_entries) - 1;
    idtr.base = idt_entries;
    asm volatile("lidt %0" ::"m"(idtr));
}

struct memmap {
    uint64_t base;
    uint64_t len;
    uint32_t type;
    uint32_t attrs;
};

void start(const struct memmap* memmap)
{
    vga_init();
    trace_init();
    setup_idt();

    TRACE("BOOTLOADER STARTED");
    unsigned mmap_count = 0;
    for(size_t i = 0; memmap[i].len && memmap[i].type; i++) {
        TRACE("memmap[%zd]: 0x%08lX 0x%08lX 0x%lX 0x%lX", i,
              (uint32_t)memmap[i].base, (uint32_t)memmap[i].len, memmap[i].type,
              memmap[i].attrs);
        mmap_count++;
    }

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
    unsigned root_dir_sectors =
        ((bpb->root_ent_count * 32) + (bpb->bytes_per_sect - 1)) /
        bpb->bytes_per_sect;
    unsigned entries_per_sector = bpb->bytes_per_sect / 32;
    TRACE("Root dir starts at sector %d",
          bpb->rsvd_sect_count + (bpb->fat_count * bpb->fat_size16));
    struct dir_entry kernel_dir_entry = {0};
    for(unsigned cur_sector =
            bpb->rsvd_sect_count + (bpb->fat_count * bpb->fat_size16);
        ; cur_sector++) {
        struct dir_entry* entries = cached_load_sector(bpb, cur_sector);
        for(unsigned i = 0; i < entries_per_sector; i++) {
            if(entries[i].name[0] == '\0') {
                PANIC("File not found: %s (ended at %d)", FILENAME, i);
            } else if(!memcmp(entries[i].name, FILENAME, 11)) {
                memcpy(&kernel_dir_entry, &entries[i],
                       sizeof(kernel_dir_entry));
                break;
            }
        }
        if(kernel_dir_entry.name[0])
            break;
    }
    TRACE("Kernel image found at cluster 0x%X (%ld bytes)",
          kernel_dir_entry.first_cluster, kernel_dir_entry.size);

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
            TRACE("p_offset: 0x%lX, "
                  "p_vaddr: 0x%lX, "
                  "p_paddr: 0x%lX, "
                  "p_filesz: %lu, "
                  "p_memsz: %lu, "
                  "p_align: %lu",
                  phdr.p_offset, phdr.p_vaddr, phdr.p_paddr, phdr.p_filesz,
                  phdr.p_memsz, phdr.p_align);

            /* Load p_filesz from offset p_offset into p_vaddr */
            unsigned remaining = phdr.p_filesz;
            unsigned char* dst = (unsigned char*)phdr.p_vaddr;
            unsigned offset = phdr.p_offset;
            while(remaining) {
                unsigned len = remaining > bpb->bytes_per_sect
                                   ? bpb->bytes_per_sect
                                   : remaining;
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

    /* Construct multiboot info */
    struct multiboot_info* multiboot_info =
        malloc(sizeof(struct multiboot_info));
    memset(multiboot_info, 0, sizeof(struct multiboot_info));
    multiboot_info->flags = MULTIBOOT_INFO_BOOTDEV | MULTIBOOT_INFO_MEM_MAP |
                            MULTIBOOT_INFO_BOOT_LOADER_NAME;
    multiboot_info->boot_device = bpb->drive_num;
    multiboot_info->boot_loader_name =
        'R' | ('A' << 8) | ('S' << 16) | ('T' << 24);
    multiboot_info->mmap_length =
        sizeof(struct multiboot_mmap_entry) * mmap_count;
    multiboot_info->mmap_addr = malloc(multiboot_info->mmap_length);
    for(unsigned i = 0; i < mmap_count; i++) {
        multiboot_info->mmap_addr[i].size =
            sizeof(struct multiboot_mmap_entry) - sizeof(uint32_t);
        multiboot_info->mmap_addr[i].addr = memmap[i].base;
        multiboot_info->mmap_addr[i].len = memmap[i].len;
        multiboot_info->mmap_addr[i].type = memmap[i].type;
    }

    /* Jump to kernel */
    TRACE("Jumping to kernel at 0x%08lX", elf_hdr.e_entry);
    asm volatile("jmp %0"
                 :
                 : "r"(elf_hdr.e_entry), "a"(MULTIBOOT_BOOTLOADER_MAGIC),
                   "b"(multiboot_info)
                 : "memory");

    TRACE("SYSTEM HALTED");
    HALT();
}

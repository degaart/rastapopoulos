#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <io.h>
#include <runtime.h>
#include <crc32.h>
#include <debug.h>
#include <serializer.h>
#include <port.h>
#include <malloc.h>
#include <string.h>
#include "blockdrv.h"
#include "blockdrv_server.h"


/*
 * http://www.osdever.net/tutorials/view/lba-hdd-access-via-pio
 * http://wiki.osdev.org/ATA_PIO_Mode
 */
/*
 * ATA0: 0x1F0 - 0x1F7, 0x3F6 (Device control/Alternate status), IRQ14
 * ATA1: 0x170 - 0x177, 0x376, IRQ15
 */
#define ATA0_BASE                       0x1F0
#define ATA1_BASE                       0x170

#define ATA_PORT_DATA                   0x000
#define ATA_PORT_ERRINFO                0x001
#define ATA_PORT_FEATURES               0x001
#define ATA_PORT_SECTORCOUNT            0x002
#define ATA_PORT_LBALO                  0x003
#define ATA_PORT_LBAMID                 0x004
#define ATA_PORT_LBAHI                  0x005
#define ATA_PORT_SELECT                 0x006 /* drive select port */
#define ATA_PORT_COMMAND                0x007
#define ATA_PORT_STATUS                 0x007
#define ATA_PORT_DEVCTRL                0x206
#define ATA_PORT_ALTSTATUS              0x206

#define ATA_CMD_FLUSH                   0xE7
#define ATA_CMD_SELECT_MASTER           0xA0
#define ATA_CMD_SELECT_SLAVE            0xB0
#define ATA_CMD_IDENTIFY                0xEC
#define ATA_CMD_READ                    0x20

#define ATA_STATUS_ERR                  (1)
#define ATA_STATUS_DRQ                  (1 << 3)
#define ATA_STATUS_SRV                  (1 << 4)
#define ATA_STATUS_DF                   (1 << 5)
#define ATA_STATUS_RDY                  (1 << 6)
#define ATA_STATUS_BSY                  (1 << 7) 

#define ATA_CTRL_NIEN                   (1 << 1)
#define ATA_CTRL_SRST                   (1 << 2)
#define ATA_CTRL_HOB                    (1 << 7)

#define ATA_STATUS_ATAPI0               0x14
#define ATA_STATUS_ATAPI1               0xEB
#define ATA_STATUS_SATA0                0x3C
#define ATA_STATUS_SATA1                0x3C

#define SECTOR_SIZE                     512


static void ata_soft_reset(int base)
{
    int dcr = base + ATA_PORT_DEVCTRL;

    outb(dcr, ATA_CTRL_SRST);   /* Send soft reset */
    outb(dcr, 0);               /* reset bus to normal operation */
    inb(dcr);                   /* wait */
    inb(dcr);
    inb(dcr);
    inb(dcr);

    while(true)  {
        int status = inb(dcr);

        if(!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_RDY))
            break;
    }
}

struct ata_identify_data {
    int removable;
    int lba48;
    unsigned udma;
    uint32_t maxlba28;
    uint64_t maxlba48;
};
struct ata_identify_data _id;

static bool identify(struct ata_identify_data* result, int base, int drive)
{
    assert(drive == 0 || drive == 1);

    /* select drive */
    ata_soft_reset(base);
    outb(base + ATA_PORT_SELECT, drive ? 0xB0 : 0xA0);
    inb(base + ATA_PORT_DEVCTRL);
    inb(base + ATA_PORT_DEVCTRL);
    inb(base + ATA_PORT_DEVCTRL);
    inb(base + ATA_PORT_DEVCTRL);

    /* Set sectorcount, lbalo, lbamid and lbahi to 0 */
    outb(base + ATA_PORT_SECTORCOUNT, 0);
    outb(base + ATA_PORT_LBALO, 0);
    outb(base + ATA_PORT_LBAMID, 0);
    outb(base + ATA_PORT_LBAHI, 0);

    /* send identify command */
    outb(base + ATA_PORT_COMMAND, ATA_CMD_IDENTIFY);

    /* read results */
    int dcr = base + ATA_PORT_DEVCTRL;
    int status = inb(dcr);
    if(!status) {
        /* drive does not exists */
        return false;
    }

    /* wait for busy bit to clear */
    while(true) {
        status = inb(dcr);
        if(!(status & ATA_STATUS_BSY))
            break;
    }

    /* handle atapi devices that dont follow spec */
    if(inb(base + ATA_PORT_LBAMID) || inb(base + ATA_PORT_LBAHI)) {
        /* drive is not ata */
        return false;
    }

    /* poll waiting for DRQ or ERR */
    while(true) {
        status = inb(dcr);
        status &= ATA_STATUS_DRQ | ATA_STATUS_ERR;
        if(status)
            break;
    }

    if(status & ATA_STATUS_ERR) {
        return false;
    }

    /* read data. 256 16-bit values */
    uint16_t buffer[256];
    for(int i = 0; i < 256; i++)
        buffer[i] = inw(base + ATA_PORT_DATA);

    /* write results */
    result->removable = buffer[0] & (1 << 7);
    result->lba48 = buffer[83] & (1 << 10);
    result->udma = buffer[88];
    result->maxlba28 = *((uint32_t*)&buffer[60]);
    result->maxlba48 = *((uint64_t*)&buffer[100]);

    return true;
}

/*
 * TODO: Make this return the number of read sectors
 */
static int ata_read_lba28(void* buffer, size_t buffer_size,
                          int sectors,
                          int base, int slave,
                          uint32_t sector)
{
    assert(sectors > 0);
    assert(slave == 0 || slave == 1);

    uint16_t* wbuffer = buffer;
    if(buffer_size < SECTOR_SIZE * sectors)
        return -1;

    /* check BSY and DRQ. Must be clear */
    int dcr = base + ATA_PORT_DEVCTRL;
    int status = inb(dcr);
    if(status & (ATA_STATUS_BSY | ATA_STATUS_DRQ)) {
        ata_soft_reset(base);
    }

    /* Drive select + LBA28 highest nibble */
    outb(base + ATA_PORT_SELECT, 0xE0 | (slave << 4) | ((sector >> 24) & 0xF));
    outb(base + ATA_PORT_ERRINFO, 0x00);
    outb(base + ATA_PORT_SECTORCOUNT, sectors);
    outb(base + ATA_PORT_LBALO, sector & 0xFF);
    outb(base + ATA_PORT_LBAMID, (sector >> 8) & 0xFF);
    outb(base + ATA_PORT_LBAHI, (sector >> 16) & 0xFF);
    outb(base + ATA_PORT_COMMAND, ATA_CMD_READ);

    int result = 0;
    bool ignore_error = true;
    while(sectors) {
        /* 
         * wait for BSY clear and DRQ set
         * ignore ERR for first 4 iterations of the first sector
         */
        for(int i = 0;; i++) {
            if(ignore_error && i >= 4)
                ignore_error = false;

            status = inb(base + ATA_PORT_ALTSTATUS);
            if(!(status & ATA_STATUS_BSY)) {
                if(status & ATA_STATUS_DRQ)
                    break;
                else if( !ignore_error && (status & (ATA_STATUS_ERR | ATA_STATUS_DF) ) )
                    break;
            }
        }

        if(status & (ATA_STATUS_ERR | ATA_STATUS_DF)) {
            return result;
        }

        ignore_error = false;

        /* Read 256 uint16_t into buffer */
        sectors--;
        for(int i = 0; i < 256; i++) {
            *wbuffer = inw(base + ATA_PORT_DATA);
            wbuffer++;
        }

        result++;

        /* delay */
        inb(base + ATA_PORT_ALTSTATUS);
        inb(base + ATA_PORT_ALTSTATUS);
        inb(base + ATA_PORT_ALTSTATUS);
        inb(base + ATA_PORT_ALTSTATUS);
    }

    return result;
}

static void openports(int base)
{
    for(int i = ATA_PORT_DATA; i <= ATA_PORT_STATUS; i++) {
        int ret = hwportopen(base + i);
        if(ret) {
            panic("Failed to open port 0x%X", base + i);
        }
    }

    int ret = hwportopen(base + ATA_PORT_DEVCTRL);
    if(ret) {
        panic("Failed to open port 0x%X", base + ATA_PORT_DEVCTRL);
    }
}

static int perform_read(unsigned char* buffer, size_t buffer_size,
                        unsigned long long offset, int byte_count,
                        unsigned long long device_size)
{
    int read_bytes = 0;

    if(byte_count < 0 || buffer_size == 0) {
        return -1;
    } else if(byte_count == 0) {
        return 0;
    } else if(offset >= device_size) {
        return 0;
    }

    if(byte_count > device_size - offset)
        byte_count = (int32_t)(device_size - offset);

    int ret;
    while(byte_count) {
        /* 
         * If the offset or the size is not aligned, we need to do first an aligned read
         * in a temporary buffer and copy back
         */
        if((offset % SECTOR_SIZE) || (byte_count < SECTOR_SIZE)) {
            unsigned char tmp[SECTOR_SIZE];
            uint32_t sector = offset / SECTOR_SIZE;             /* sector index */
            int offset_in_sector = offset % SECTOR_SIZE;        /* offset we need inside sector */
            int data_size = SECTOR_SIZE - offset_in_sector;     /* size of data we need */
            if(data_size > buffer_size)
                data_size = buffer_size;
            if(data_size > byte_count)
                data_size = byte_count;

            ret = ata_read_lba28(tmp, sizeof(tmp), 
                                 1, 
                                 ATA0_BASE, 0, 
                                 sector);
            if(ret != 1) {
                return read_bytes;
            }

            memcpy(buffer, tmp + offset_in_sector, data_size);
            offset += data_size;

            buffer += data_size;
            byte_count -= data_size;
            read_bytes += data_size;
            buffer_size -= data_size;
        }

        /*
         * Read maximum number of whole-sectors we can read
         */
        if(byte_count >= SECTOR_SIZE) {
            int wholesectors = byte_count / SECTOR_SIZE;
            uint32_t start_sector = offset / SECTOR_SIZE;

            ret = ata_read_lba28(buffer, buffer_size,
                                 wholesectors,
                                 ATA0_BASE, 0,
                                 start_sector);
            if(ret <= 0) {
                return read_bytes;
            }

            unsigned data_size = wholesectors * SECTOR_SIZE;
            offset += data_size;
            buffer += data_size;
            byte_count -= data_size;
            read_bytes += data_size;
            buffer_size -= data_size;
        }

        assert(byte_count >= 0);
    }

    return read_bytes;
}

/*
 * Read sector
 * Params:
 *  buffer          Buffer
 *  buffer_size     Size of buffer in bytes. Must be >= SECTOR_SIZE
 *  int             Sector index
 * Returns
 *  0 in case of success.
 *  Any other value denotes failure.
 */
int handle_blkimpl_read_sector(int sender_pid, 
                               /* out */ void* buffer, 
                               /* in, out */ size_t* buffer_size, 
                               int sector)
{
    if(*buffer_size < SECTOR_SIZE) {
        *buffer_size = 0;
        return -1;
    }

    int read_ret = ata_read_lba28(buffer, *buffer_size,
                                  1,
                                  ATA0_BASE, 0,
                                  sector);
    if(read_ret != 1) {
        *buffer_size = 0;
        return -1;
    } else {
        *buffer_size = SECTOR_SIZE;
        return 0;
    }
}

/*
 * Get sector count
 * Params: none
 * Returns:
 *  Sector count
 *  0xFFFFFFFF in case of error
 */
int handle_blkimpl_sector_count(int sender_pid)
{
    return _id.maxlba28;
}

/*
 * Get total size of block device
 * Returns:
 *  long long   Total size of block device in bytes
 */
long long handle_blkimpl_total_size(int sender_pid)
{
    return (long long)_id.maxlba28 * SECTOR_SIZE;
}

/*
 * Read bytes from block device
 * Params:
 *  buffer          Output buffer
 *  buffer_size     Number of bytes to read
 *  offset          Offset
 * Returns:
 *  Number of bytes actually read.
 *  0 on end of device
 *  -1 in case of error
 */
int handle_blkimpl_read(int sender_pid, 
                        /* out */ void* buffer, 
                        /* in, out */ size_t* buffer_size, 
                        int offset)
{
    unsigned long long device_size = (unsigned long long)_id.maxlba28 * SECTOR_SIZE;
    int result = perform_read(buffer, *buffer_size,
                              offset, *buffer_size,
                              device_size);
    if(result >= 0)
        *buffer_size = result;
    else
        *buffer_size = 0;
    return result;
}

void main()
{
    int ret;

    ret = port_open(BlkPort);
    if(ret < 0) {
        panic("Failed to open BlockDrv port");
    }

    trace("Opening I/O ports");
    openports(ATA0_BASE);

    trace("Detecting Primary Master");
    bool success = identify(&_id, ATA0_BASE, 0);
    if(!success) {
        panic("ATA Primary Master not detected");
    }

    trace("Block driver started");
    rpc_dispatch(BlkPort);

    panic("Invalid code path");

}


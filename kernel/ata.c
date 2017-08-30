#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "io.h"
#include "kdebug.h"

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

static int ata_read_lba28(void* buffer, size_t buffer_size,
                          int sectors,
                          int base, int slave,
                          uint32_t sector)
{
    assert(sectors > 0);
    assert(slave == 0 || slave == 1);

    uint16_t* wbuffer = buffer;
    if(buffer_size < 512 * sectors)
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
            result += 2;
        }

        /* delay */
        inb(base + ATA_PORT_ALTSTATUS);
        inb(base + ATA_PORT_ALTSTATUS);
        inb(base + ATA_PORT_ALTSTATUS);
        inb(base + ATA_PORT_ALTSTATUS);
    }

    return result;
}

void test_ata()
{
    trace("Detecting Primary Master");
    struct ata_identify_data id;
    bool success = identify(&id, ATA0_BASE, 0);
    if(!success) {
        trace("\tNot Detected");
    } else {
        trace("\tDetected. LBA48: %s", id.lba48 ? "true": "false");

        trace("Reading first sector of primary master");

        unsigned char buffer[512];
        int ret = ata_read_lba28(buffer, sizeof(buffer),
                                 1,
                                 ATA0_BASE, 0,
                                 0);
        if(ret < 512) {
            panic("Read failed. Ret: %d", ret);
        }

        trace("buffer[0]: %X", (int)buffer[0]);
        trace("buffer[1]: %X", (int)buffer[1]);
        trace("buffer[2]: %X", (int)buffer[2]);
        trace("buffer[3]: %X", (int)buffer[3]);

        assert(buffer[0] == 0xEB);
        assert(buffer[1] == 0x63);
        assert(buffer[2] == 0x90);
        assert(buffer[3] == 0x00);
        assert(buffer[510] == 0x55);
        assert(buffer[511] == 0xAA);
    }
}


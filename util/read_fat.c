#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FAT_OK 0
#define FAT_IOERROR 1
#define FAT_UNSUPPORTED 2
#define FAT_NOTFOUND 3

struct FAT_BPB {
	uint8_t unused1[11];
	uint16_t bps;
	uint8_t spc;
	uint16_t rsc;
	uint8_t fats;
	uint16_t rentcnt;
	uint16_t sec16;
	uint8_t media;
	uint16_t fatsz16;
	uint16_t spt;
	uint16_t nheads;
	uint32_t hsect;
	uint32_t sec32;
	
	uint32_t fatsz32;
	uint16_t eflags;
	uint16_t fsver;
	uint32_t rclus;
	uint16_t fsinfo;
	uint16_t bkbr;
	uint8_t unused2[12];
	uint8_t drvnum;
	uint8_t unused3[1];
	uint8_t bootsig;
	uint32_t volid;
	uint8_t vollab[11];
	uint8_t systype;
} __attribute__((packed));

struct FAT {
	uint16_t device;
	uint16_t type;
	
	uint16_t cyl;
	uint16_t head;
	uint16_t sect;
	uint16_t spt;

	uint32_t rsect;
	uint32_t fatsz;
	uint32_t scount;
	uint16_t spc;
	uint16_t rsc;
	uint16_t fatc;
	uint16_t rentries;
	uint16_t hsc;
	uint32_t rclus;
};

FILE* device;
uint8_t workmem[1024];

uint16_t fat_read_chs(uint16_t device, uint8_t* buffer, uint16_t c, uint16_t h, uint16_t s) {
	





}

uint16_t fat_open(uint16_t device, struct FAT* hfat) {
	/*
		read boot record
		It is at C=0, H=0, S=1
	*/
	uint16_t ret = fat_read_chs(device, workmem, 0, 0, 1);
	fat_error(ret);
		
	/* Dump some values to make sure we're ok */
	fat_dump_bpb(workmem);
	
	/* Determine count of sectors occupied by root dir */
	return(FAT_OK);
}

void fat_error(int ret) {
	switch(ret) {
	case FAT_OK:
		break;
	case FAT_IOERROR:
		fprintf(stderr, "IO Error\n");
		exit(1);
	case FAT_UNSUPPORTED:
		fprintf(stderr, "UNSUPPORTED Error\n");
		exit(1);
	case FAT_NOTFOUND:
		fprintf(stderr, "NOTFOUND Error\n");
		exit(1);
	default:
		fprintf(stderr, "UNKNOWN Error\n");
		exit(1);
	}
}
	
int main() {
	device=fopen("floppy.img", "rb");
	if(!device) {
		fprintf(stderr, "Cannot open floppy.img\n");
		return(1);
	}
	
	struct FAT fat;
	uint16_t ret = fat_open(0, &fat);
	fat_error(ret);

	fclose(device);	
	return(0);
}


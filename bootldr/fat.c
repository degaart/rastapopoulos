#include <stdint.h>
#include "bootldr_stub.h"
#include "disk.h"
#include "fat.h"
#include "bootldr_str.h"

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

static uint8_t fat_workmem[512*2];
static uint16_t* fat_signature = (uint16_t*)(&fat_workmem[510]);
static struct FAT_BPB* fat_bpb = (struct FAT_BPB*)fat_workmem;

const char* fat_error(uint16_t code) {
	switch(code) {
	case FAT_OK:
		return("OK");
	case FAT_UNSUPPORTED:
		return("UNSUPPORTED");
	case FAT_IOERROR:
		return("IOERROR");
	case FAT_NOTFOUND:
		return("NOTFOUND");
	case FAT_EOF:
		return("EOF");
	default:
		return("UNKNOWN");
	}
}

void fat_dump_bpb(struct FAT_BPB* bpb) {
	DUMP16(bpb->bps);
	DUMP16(bpb->spc);
	DUMP16(bpb->rsc);
	DUMP16(bpb->fats);
	DUMP16(bpb->rentcnt);
	DUMP16(bpb->sec16);
	DUMP16(bpb->spt);
	DUMP16(bpb->fatsz16);
	DUMP16(bpb->nheads);
}

uint16_t fat_open(struct FAT* hfat, uint16_t device) {
	/* Read first sector of device */
	uint16_t ret = disk_read_chs(fat_workmem, device, 0, 0, 1);
	if(!ret)
		return(FAT_IOERROR);
	
	/* Check signature */
	if(*fat_signature != 0xAA55) {
		return(FAT_UNSUPPORTED);
	}
	
	/* Determine fat type */
	uint32_t root_dir_sectors = ((fat_bpb->rentcnt*32)+(fat_bpb->bps-1))/fat_bpb->bps;
	if(fat_bpb->fatsz16 != 0)
		hfat->fatsz = fat_bpb->fatsz16;
	else
		hfat->fatsz = fat_bpb->fatsz32;

	if(fat_bpb->sec16 == 0)
		hfat->scount = fat_bpb->sec32;
	else
		hfat->scount = fat_bpb->sec16;

	uint32_t data_sectors = hfat->scount - (fat_bpb->rsc + (fat_bpb->fats*hfat->fatsz) + root_dir_sectors);
	uint32_t cluster_count = data_sectors / fat_bpb->spc;
	if(cluster_count < 4085)
		hfat->type = FAT_TYPE_FAT12;
	else if(cluster_count < 65525)
		hfat->type = FAT_TYPE_FAT16;
	else
		hfat->type = FAT_TYPE_FAT32;
	
	/* pre-fill buffer */
	hfat->device = device;
	if(fat_bpb->sec16 == 0)
		hfat->cyl = fat_bpb->sec32/(fat_bpb->spt * fat_bpb->nheads);
	else
		hfat->cyl = fat_bpb->sec16/(fat_bpb->spt * fat_bpb->nheads);
	hfat->head = fat_bpb->nheads;
	hfat->spt = fat_bpb->spt;
	hfat->sect = fat_bpb->bps;
	
	hfat->rsect = fat_bpb->rsc;
	hfat->spc = fat_bpb->spc;
	hfat->rsc = fat_bpb->rsc;
	hfat->fatc = fat_bpb->fats;
	hfat->rentries = fat_bpb->rentcnt;
	hfat->hsc = fat_bpb->hsect;

	/* Get root dir sector number */
	if(hfat->type == FAT_TYPE_FAT32) {
		hfat->rsector = fat_bpb->rclus / fat_bpb->spc;
	} else {
		hfat->rsector = fat_bpb->rsc + (fat_bpb->fats * fat_bpb->fatsz16);
	}
	
	/* Get first data sector */
	hfat->dsect_start =  fat_bpb->rsc + (fat_bpb->fats * fat_bpb->fatsz16) + root_dir_sectors;

	if(hfat->type == FAT_TYPE_FAT12)
		return(FAT_OK);
	else
		return(FAT_UNSUPPORTED);
}

uint16_t fat_read_lsect(void* buffer, struct FAT* hfat, uint32_t lsect) {
	uint16_t sector;
	uint16_t head;
	uint16_t cyl;
	
	fat_lsect_to_chs(hfat, lsect, &cyl, &head, &sector);
	uint16_t read = disk_read_chs(buffer, hfat->device, cyl, head, sector);
	if(read)
		return(FAT_OK);
	else
		return(FAT_IOERROR);
}

void fat_lsect_to_chs(struct FAT* hfat, uint32_t lsect, uint16_t* cyl, uint16_t* head, uint16_t* sector) {
	/*
		; 	temp = lsect / spt
		;	s = (lsect % spt) + 1
		;	h = temp % heads
		;	c = temp / heads
	*/
	uint16_t temp = lsect / hfat->spt;
	*sector = (lsect % hfat->spt) + 1;
	*head = temp % hfat->head;
	*cyl = temp / hfat->head;
}

uint16_t fat_fopen(struct FAT_FILE* hfile, struct FAT* hfat, const char* filename) {
	if(hfat->type != FAT_TYPE_FAT12)
		return(FAT_UNSUPPORTED);
	
	/* Scan root dir */
	uint32_t current_sector = hfat->rsector;	/* Current sector we're reading */
	uint32_t entries_read = 0;					/* Entries read so far */
	uint16_t current_entry;						/* Current entry we're reading */	
	uint16_t ret;
	uint16_t entries_per_cluster = hfat->sect / 32;
	uint32_t first_cluster = 0;					/* First cluster of file */
	uint32_t file_size;						/* File size */
	
	do {
		/* Load sector */
		ret = fat_read_lsect(fat_workmem, hfat, current_sector);
		if(ret != FAT_OK)
			return(ret);
		current_entry = 0;
		do {
			if(!memcmp(&fat_workmem[current_entry*32], filename, 8+3)) {
				uint16_t* loword = (uint16_t*)&fat_workmem[(current_entry*32)+26];
				uint16_t* hiword = (uint16_t*)&fat_workmem[(current_entry*32)+20];
				uint32_t* psize = (uint32_t*)&fat_workmem[(current_entry*32)+28];
				
				first_cluster = MAKEDWORD(*loword, *hiword);
				file_size = *psize;
				break;
			}
				
			/* next entry */
			current_entry++;
		} while((!first_cluster) && (current_entry < entries_per_cluster));

		/* Next sector */
		current_sector++;
	} while((!first_cluster) && (entries_read < hfat->rentries));
	
	if(!first_cluster) {
		return(FAT_NOTFOUND);
	}
	
	hfile->first_cluster = first_cluster;
	hfile->current_cluster = first_cluster;
	hfile->size = file_size;
	return(FAT_OK);
}

uint32_t fat_next_cluster12(struct FAT* hfat, uint32_t cluster) {
	/* Determine offset and sector of fat entry */
	uint32_t offset = cluster + (cluster / 2);
	uint32_t sector = hfat->rsc + (offset / hfat->sect);
	uint32_t entry_offset = offset % hfat->sect;

	/* Load this sector and the subsequent sector */
	uint16_t ret = fat_read_lsect(fat_workmem, hfat, sector);
	if(ret != FAT_OK)
		return(ret);
	ret = fat_read_lsect(fat_workmem+512, hfat, sector+1);
	
	/* Get value */
	uint16_t entry = *((uint16_t*)(&fat_workmem[offset]));
	if(cluster & 0x0001)
		entry = entry >> 4;			/* odd cluster number */
	else
		entry = entry & 0x0FFF; 	/* even cluster number */
	return(entry);
}

uint16_t fat_fread(void* buffer, struct FAT* hfat, struct FAT_FILE* hfile) {
	if(hfat->type != FAT_TYPE_FAT12)
		return(FAT_UNSUPPORTED);
	if(hfat->spc != 1)
		return(FAT_UNSUPPORTED);
		
	/* If starting cluster is 0, then it's an empty file */
	if(!hfile->first_cluster)
		return(FAT_EOF);
	
	/* Determine if we're at end of file */
	if(hfile->current_cluster >= 0x0FF8)
		return(FAT_EOF);
	
	/* Map current cluster into a disk sector */
	uint32_t cluster_sector = ((hfile->current_cluster - 2) * hfat->spc) + hfat->dsect_start;
	
	/* Read sector */
	uint16_t ret = fat_read_lsect(buffer, hfat, cluster_sector);
	if(ret != FAT_OK)
		return(ret);
	
	/* Determine next cluster */
	if(hfat->type == FAT_TYPE_FAT12)
		hfile->current_cluster = fat_next_cluster12(hfat, hfile->current_cluster);
	return(FAT_OK);
}



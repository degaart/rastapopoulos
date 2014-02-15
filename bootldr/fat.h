#ifndef _FAT_H_
#define _FAT_H_

	#define FAT_OK 0
	#define FAT_UNSUPPORTED 1
	#define FAT_NOTFOUND 2
	#define FAT_IOERROR 3
	#define FAT_EOF 4
	
	#define FAT_TYPE_FAT12 0
	#define FAT_TYPE_FAT16 1
	#define FAT_TYPE_FAT32 2

	struct FAT {
		uint16_t device;
		uint16_t type;
		
		uint16_t cyl;			/* Cyl count */
		uint16_t head;			/* Head count */
		uint16_t sect;			/* Sector size */
		uint16_t spt;			/* Sector per track */
	
		uint32_t rsect;
		uint32_t fatsz;
		uint32_t scount;		/* Total sector count */
		uint16_t spc;			/* sectors per cluster */
		uint16_t rsc;			/* Reserved sector count */
		uint16_t fatc;
		uint16_t rentries;		/* Entries in root directory */
		uint16_t hsc;
		uint32_t rsector;		/* Root directory sector number */
		uint32_t dsect_start;	/* First data sector */
	} __attribute__((packed));
	
	struct FAT_FILE {
		uint32_t first_cluster;
		uint32_t current_cluster;
		uint32_t size;
	} __attribute((packed));
	
	const char* fat_error(uint16_t code);
	uint16_t fat_open(struct FAT* hfat, uint16_t device);
	uint16_t fat_fopen(struct FAT_FILE* hfile, struct FAT* hfat, const char* filename);
	void fat_lsect_to_chs(struct FAT* hfat, uint32_t lsect, uint16_t* cyl, uint16_t* head, uint16_t* sector);	
	uint16_t fat_read_lsect(void* buffer, struct FAT* hfat, uint32_t lsect);
	uint32_t fat_next_cluster12(struct FAT* hfat, uint32_t cluster);
	uint16_t fat_fread(void* buffer, struct FAT* hfat, struct FAT_FILE* hfile);
	
#endif //_FAT_H_

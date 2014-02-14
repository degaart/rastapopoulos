#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

FILE* data;
uint8_t workmem[512*2];
uint8_t bpb[512];
uint8_t fat_table[29696];
uint8_t file_buffer[65536];

uint16_t* rsectcount = (uint16_t*)(bpb+0x000E); /* reserved sectors */
uint8_t* fatcount = (uint8_t*)(bpb+0x000F); /* Number of FATs */
uint16_t* direntcount = (uint16_t*)(bpb+0x0011); /* Dir entries in root dir. WARNING: ce fut faux dans la version asm */
uint16_t* sectcount = (uint16_t*)(bpb+0x0013);
uint16_t* spf = (uint16_t*)(bpb+0x0016);
uint16_t* spt = (uint16_t*)(bpb+0x0018);
uint16_t* headcount = (uint16_t*)(bpb+0x001A);
uint16_t* hsectcount = (uint16_t*)(bpb+0x001C);

#define FILENAME "KERNEL     "

void read_lsect(void* buffer, unsigned lsect) {
	if(fseek(data, (lsect-1)*512, SEEK_SET)) {
		fprintf(stderr, "Seek error\n");
		exit(1);
	}
	if(fread(buffer, 512, 1, data) < 1) {
		fprintf(stderr, "Read error\n");
		exit(1);
	}
}

void dump(void* buffer, int size) {
	for(int i=0; i<size; i++) {
		if(i % 16 == 0)
			printf("\n");
		printf("%02X ", (int)( (((uint8_t*)buffer)[i]) ));
	}
	printf("\n");
}

/* Read file into buffer */
int read_file(void* buffer, unsigned starting_sector,uint32_t size) {
	unsigned root_dir_sectors = ((*direntcount * 32) + (512-1)) / 512;
	unsigned first_data_sector = *rsectcount + (2 * *spf) + root_dir_sectors;
	printf(
		"root_dir_sectors: %u\n"
		"first_data_sector: %u\n",
		root_dir_sectors,
		first_data_sector
	);

	/* Read starting from current sector */
	unsigned current_sector = starting_sector;
	uint8_t* current_buffer = (uint8_t*)buffer;
	int read_sectors = 0;
	while(1) {
		read_sectors++;
		printf("Current sector: %u, Reading sector %u\n", current_sector, (current_sector-2) + first_data_sector + 1);
		read_lsect(current_buffer, (current_sector-2) + first_data_sector + 1);
		
		unsigned fat_offset = current_sector + (current_sector / 2);
		uint16_t next_sector = *((uint16_t*)(fat_table+fat_offset));
		if(current_sector & 0x1) { /* Odd */
			/* Keep high order 12 bits */
			next_sector = next_sector >> 4;
		} else {
			/* Keep low order 12 bits */
			next_sector = next_sector & 0x0FFF;
		}
		if(next_sector >= 0xFF8) {
			break;
		}
		current_sector = next_sector;
        current_buffer += 512;
    }
	return(read_sectors);
}

int main() {
	data = fopen("/Volumes/Kratos/Projets/RastapopoulOS/src/0.09/floppy.img","rb");
	if(!data)
		return(1);
    
	/* Load boot sector in bootsect */
	printf("Loading BPB\n");
	read_lsect(bpb, 1);
	//dump(bpb, 512);
	
	/* Load fat at fat_table, because we're lazy */
	printf("Loading FAT\n");
	for(unsigned sect=2; sect<2+ *spf; sect++) {
		read_lsect(fat_table+((sect-2)*512), sect);
	}
	
	/*
     ; calculate root dir sector
     ; (number of fats * sectors/fat) + hidden sectors + reserved sectors
     ; assume number of fats=2
     ; The correct value should be 20
     */
	printf(
           "{ spt:%u, spf: %u, hsectcount: %u, rsectcount: %u, rootdirsiz: %u }\n",
           (unsigned)(*spt),
           (unsigned)(*spf),
           (unsigned)(*hsectcount),
           (unsigned)(*rsectcount),
           (unsigned)(*direntcount)
           );
	unsigned root_dir_sect = (2 * *spf) + *hsectcount + *rsectcount + 1;
	printf("root_dir_sect=%u\n", root_dir_sect);
	
	/* Find directory entry for BOOTLDR */
	uint8_t* bootldr_entry = 0;
	for(unsigned sect=root_dir_sect; sect<root_dir_sect+ *spf; sect++) {
		read_lsect(workmem, sect);
		
		/* Read each fat entry from sector */
		int end = 0;
		for(unsigned entry=0; entry<16; entry++) {
			/* Compare filename */
			if(workmem[entry*32] == 0) {
				/* End of entries */
				end = 1;
				break;
			} else if(!memcmp(&workmem[entry*32], FILENAME, 8+3)) {
				end = 1;
				bootldr_entry = workmem+(entry*32);
				break;
			}
		}
		if(end)
			break;
	}
	
	if(!bootldr_entry) {
		printf(FILENAME " missing\n");
		return(1);
	}
	
	/* Now we have starting cluster and size of BOOTLDR */
	uint16_t bootldr_start_cluster = *(uint16_t*)(bootldr_entry+26);
	uint32_t bootldr_size = *(uint32_t*)(bootldr_entry+28);
	printf(FILENAME " starts at cluster 0x%04X\n", (unsigned)(bootldr_start_cluster));
	printf(FILENAME " size: %u\n", (unsigned)(bootldr_size));
	
	//unsigned first_data_sector = ((*spf *2)+((*direntcount * 32)/512)+ *rsectcount);
	//unsigned root_dir_sectors = ((*direntcount * 32) + (512-1)) / 512;
	//unsigned first_data_sector = *rsectcount + (2 * *spf) + root_dir_sectors;
	//printf("First data sector: %u\n", first_data_sector);
    
	/* read first cluster of BOOTLDR */
	//read_lsect(workmem, (bootldr_start_cluster-2) + first_data_sector + 1);
	//read_lsect(workmem, bootldr_start_cluster + first_data_sector);
	//dump(workmem, 512);
	int sectors = read_file(file_buffer, bootldr_start_cluster, bootldr_size);
	//dump(file_buffer, sectors*512);
	return(0);
}


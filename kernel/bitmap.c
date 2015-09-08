#include <stdint.h>
#include "kutil.h"
#include "kterm.h"
#include "kstring.h"
#include "bitmap.h"

/*
 index: index of the bit
 */
void bitmap_set(struct BITMAP* bitmap, unsigned index, unsigned value) {
    ASSERT(index < bitmap->bitcount);
    ASSERT((index / 32) < bitmap->elcount);
    
    unsigned mask = 1<<(index % 32);
    if(value) {
        bitmap->data[index / 32] |= mask;
	} else {
		bitmap->data[index / 32] &= ~mask;
    }
}

/*
 index: index of the bit
 */
unsigned bitmap_get(const struct BITMAP* bitmap, unsigned index) {
	if(index >= bitmap->bitcount) {
		write_string("index >= bitmap->bitcount ");
		DUMP32(index);
		DUMP32(bitmap->bitcount);
	}
    ASSERT(index < bitmap->bitcount);

	if((index/32) >= bitmap->elcount) {
		write_string("(index/32) >= bitmap->elcount ");
		DUMP32(index/32);
		DUMP32(bitmap->elcount);
	}    
    ASSERT((index / 32) < bitmap->elcount);
    
    unsigned ret = bitmap->data[index / 32] & (1 << (index % 32));    /* TODO: Remove right shift */
	return(ret);
}

/*
 bitmap_size: Bitmap element count
 return: index of the first bit that is unset
 or UINT32_MAX if no free bits found
 */
unsigned bitmap_find_free(const struct BITMAP* bitmap) {
    unsigned i;
	for(i=0; i<bitmap->bitcount/32; i++) {
		if(bitmap->data[i] != 0xFFFFFFFF) {
			for(unsigned j=i*32; j<(i+1)*32; j++) {
				if(!bitmap_get(bitmap, j)) {
					return(j);
				}
			}
		}
	}
    
    /* Wait! there's still memory left to scan! */
    for(unsigned k=i*32; k<bitmap->bitcount; k++) {
        if(!bitmap_get(bitmap, k))
            return(k);
    }
	return(UINT32_MAX);
}

static unsigned bitmap_contiguous(const struct BITMAP* bitmap, unsigned start_bit, unsigned region_size) {
    unsigned contiguous = 1;
    for(unsigned bit = start_bit; bit < start_bit+region_size; bit++) {
        if(bitmap_get(bitmap, bit)) {
            contiguous = 0;
            break;
        }
    }
    return(contiguous);
}

/*
 find first region of the given size
 returns index of first bit of the region
 or UINT32_MAX if no free region found
 NOTE: bitmap_size is number of elements in bitmap
 */
unsigned bitmap_find_free_region(const struct BITMAP* bitmap, unsigned region_size) {
    ASSERT(region_size != 0);
    
    if(region_size == 1)
        return(bitmap_find_free(bitmap));

    if(region_size > bitmap->bitcount)
        return(UINT32_MAX);

    /* Begin to scan beginning at free_bit */
    for(unsigned j=0; j<(bitmap->bitcount)-region_size; j++) {
        if(bitmap_contiguous(bitmap, j, region_size))
            return(j);
    }
    return(UINT32_MAX);
}

/*
    Size: bitmap element count
 */
static void bitmap_dump(const struct BITMAP* bitmap) {
#ifdef __APPLE__
    for(unsigned i=0; i<bitmap->elcount; i++) {
        printf("%08X ", bitmap->data[i]);
    }
    printf("\n");
#endif
}

static void bitmap_dump_bits(const struct BITMAP* bitmap, unsigned size) {
#ifdef __APPLE__
    for(size_t i=0; i<size*32; i++) {
        printf("%c", bitmap_get(bitmap, i)?'1':'0');
    }
    printf("\n");
#endif
}

/*
	Get size of memory in bytes required to
	store a bitmap containaing the specified number of bits
*/
uint32_t bitmap_get_storage_size(uint32_t bitcount) {
    if(bitcount % 32 == 0)
        return( (bitcount / 32) * 4 );
    else
        return( ((bitcount / 32) + 1) * 4 );
}

void bitmap_init(struct BITMAP* bitmap, uint32_t bitcount, void* storage) {
    bitmap->bitcount = bitcount;
    bitmap->data = storage;
    bitmap->size_bytes = bitmap_get_storage_size(bitcount);
    bitmap->elcount = bitmap->size_bytes/4;
    bitmap_clear(bitmap);
    
    ASSERT(bitmap->elcount*32 >= bitmap->bitcount);
}

/*
	Unset all the bits in the bitmap
*/
void bitmap_clear(struct BITMAP* bitmap) {
	bzero(bitmap->data, bitmap->size_bytes);
}

/*
	Set all the bits in the bitmap
*/
void bitmap_fill(struct BITMAP* bitmap) {
	memset(bitmap->data, 0xFF, bitmap->size_bytes);
}


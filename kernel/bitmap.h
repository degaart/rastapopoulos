#ifndef _BITMAP_H_
#define _BITMAP_H_

	struct BITMAP {
		uint32_t bitcount;          /* bitmap size, in bits */
		uint32_t elcount;           /* Element count */
		uint32_t size_bytes;        /* Bitmap size, in bytes */
		uint32_t* data;
	};
	void bitmap_set(struct BITMAP* bitmap, unsigned index, unsigned value);
	unsigned bitmap_get(const struct BITMAP* bitmap, unsigned index);
	unsigned bitmap_find_free(const struct BITMAP* bitmap);
	unsigned bitmap_find_free_region(const struct BITMAP* bitmap, unsigned region_size);
	uint32_t bitmap_get_storage_size(uint32_t bitcount);
	void bitmap_init(struct BITMAP* bitmap, uint32_t bitcount, void* storage);

#endif //_BITMAP_H_


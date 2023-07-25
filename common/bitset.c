#include "bitset.h"
#include "debug.h"
#include "string.h"
#include "util.h"

#define BITS 32

struct bitset {
  unsigned count;
  uint32_t data[];
};

/* Return the amount of bytes necessary to store a bitset for count bits */
size_t bitset_get_size(unsigned count) {
  return sizeof(struct bitset) + (ALIGN(count, BITS) / BITS * sizeof(uint32_t));
}

void bitset_init(struct bitset *bitset, size_t size, unsigned count) {
  assert(size >= bitset_get_size(count));
  memset(bitset, 0, size);
  bitset->count = count;
}

void bitset_set_value(struct bitset *bitset, unsigned index, bool value) {
  assert(index < bitset->count);
  size_t offset = index / BITS;
  size_t bit_offset = index % BITS;
  if (value) {
    bitset->data[offset] |= 1 << bit_offset;
  } else {
    bitset->data[offset] &= ~(1 << bit_offset);
  }
}

bool bitset_get(const struct bitset *bitset, unsigned index) {
  assert(index < bitset->count);
  size_t offset = index / BITS;
  size_t bit_offset = index % BITS;
  return bitset->data[offset] & (1 << bit_offset);
}

void bitset_set(struct bitset *bitset, unsigned index) {
  assert(index < bitset->count);
  bitset_set_value(bitset, index, true);
}

void bitset_clear(struct bitset *bitset, unsigned index) {
  assert(index < bitset->count);
  bitset_set_value(bitset, index, false);
}

/*
 * Finds the index of the first cleared bit
 * Returns BITSET_INVALID if no cleared bit found
 */
size_t bitset_find(struct bitset *bitset) {
  unsigned data_count = ALIGN(bitset->count, BITS) / BITS;
  for (size_t i = 0; i < data_count; i++) {
    if (bitset->data[i] == 0x00000000) {
      return i * BITS;
    } else if (bitset->data[i] != 0xFFFFFFFF) {
      for (unsigned j = 0; j < BITS; j++) {
        if (!(bitset->data[i] & (1 << j))) {
          if ((i * BITS) + j < bitset->count)
            return (i * BITS) + j;
          else
            return BITSET_INVALID;
        }
      }
    }
  }
  return BITSET_INVALID;
}

void bitset_test() {
  /* Test bitset_get_size */
  assert(bitset_get_size(0) == sizeof(struct bitset));
  assert(bitset_get_size(1) == sizeof(struct bitset) + sizeof(uint32_t));
  assert(bitset_get_size(32) == sizeof(struct bitset) + sizeof(uint32_t));
  assert(bitset_get_size(33) ==
         sizeof(struct bitset) + sizeof(uint32_t) + sizeof(uint32_t));

  /* Test bitset_init */
  static unsigned char storage1[sizeof(struct bitset) + sizeof(uint32_t)];
  struct bitset *bs1 = (struct bitset *)storage1;
  bitset_init(bs1, sizeof(storage1), 32);
  assert(bs1->count == 32);
  assert(bs1->data[0] == 0);

  /* Test bitset_set_value */
  static unsigned char storage2[sizeof(struct bitset) + (sizeof(uint32_t) * 2)];
  struct bitset *bs2 = (struct bitset *)storage2;
  bitset_init(bs2, sizeof(storage2), 33);

  bitset_set(bs2, 0);
  assert(bs2->data[0] == 0x01);
  assert(bs2->data[1] == 0x00);

  bitset_clear(bs2, 0);
  assert(bs2->data[0] == 0x00);
  assert(bs2->data[1] == 0x00);

  bitset_set(bs2, 1);
  assert(bs2->data[0] == 0x02);
  assert(bs2->data[1] == 0x00);

  bitset_clear(bs2, 1);
  assert(bs2->data[0] == 0x00);
  assert(bs2->data[1] == 0x00);

  bitset_set(bs2, 32);
  assert(bs2->data[0] == 0x00);
  assert(bs2->data[1] == 0x01);

  for (size_t i = 0; i < 33; i++) {
    bitset_set(bs2, i);
  }
  assert(bs2->data[0] == 0xFFFFFFFF);
  assert(bs2->data[1] == 0x00000001);

  for (size_t i = 0; i < 33; i++) {
    bitset_clear(bs2, i);
  }
  assert(bs2->data[0] == 0x00000000);
  assert(bs2->data[1] == 0x00000000);

  /* Test bitset_find */
  static unsigned char storage3[sizeof(struct bitset) + (sizeof(uint32_t) * 2)];
  struct bitset *bs3 = (struct bitset *)storage3;
  bitset_init(bs3, sizeof(storage2), 33);

  assert(bitset_find(bs3) == 0);

  bitset_set(bs3, 0);
  assert(bitset_find(bs3) == 1);

  for (size_t i = 0; i < 32; i++) {
    bitset_set(bs3, i);
  }
  assert(bitset_find(bs3) == 32);
  bitset_set(bs3, 32);

  assert(bitset_find(bs3) == BITSET_INVALID);
}

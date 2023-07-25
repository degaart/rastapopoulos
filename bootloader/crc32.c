#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CRC32_INITIAL 0xFFFFFFFF
static uint32_t crc32(uint32_t state, const void *buffer, size_t size) {
  uint32_t r = state;
  const unsigned char *data = buffer;
  while (size--) {
    r ^= *data++;

    for (int i = 0; i < 8; i++) {
      uint32_t t = ~((r & 1) - 1);
      r = (r >> 1) ^ (0xEDB88320 & t);
    }
  }

  return r;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
    return 1;
  }

  const char *filename = argv[1];
  FILE *f = fopen(filename, "rb");
  if (!f) {
    perror("open");
    return 1;
  }

  unsigned char buffer[512];
  uint32_t crc = CRC32_INITIAL;
  size_t total = 0;
  while (1) {
    size_t r = fread(buffer, 1, sizeof(buffer), f);
    printf("r: %zd\n", r);
    if (ferror(stdin)) {
      perror("read");
      return 1;
    } else if (!r) {
      break;
    }
    crc = crc32(crc, buffer, r);
    total += r;
  }
  crc = ~crc;
  printf("0x%08X %zu bytes\n", crc, total);
  return 0;
}

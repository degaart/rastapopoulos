#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

  return ~r;
}

char *rtrim(char *buf) {
  if (!*buf)
    return buf;
  char *ptr = buf + strlen(buf) - 1;
  while (ptr > buf && isspace(*ptr)) {
    *ptr = '\0';
    ptr--;
  }
  return buf;
}

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <kernel> <crc-list>\n", argv[0]);
    return 1;
  }

  const char *kernel_file = argv[1];
  const char *crc_list_file = argv[2];

  FILE *kernel = fopen(kernel_file, "rb");
  if (!kernel) {
    perror("open");
    return 1;
  }

  FILE *crc_list = fopen(crc_list_file, "rt");
  if (!crc_list) {
    perror("open");
    return 1;
  }

  char line[4096];
  char buffer[4096];
  while (1) {
    if (!fgets(line, sizeof(line), crc_list)) {
      if (ferror(crc_list)) {
        perror("read");
        return 1;
      } else {
        break;
      }
    }
    rtrim(line);
    if (!strlen(line))
      continue;

    char *ptr;

    const char *soffset = line;
    ptr = strchr(line, ',');
    assert(ptr != NULL);
    *ptr = '\0';

    const char *slen = ptr + 1;
    ptr = strchr(slen, ',');
    assert(ptr != NULL);
    *ptr = '\0';

    const char *scrc = ptr + 1;

    unsigned long offset = strtoul(soffset, NULL, 10);
    unsigned long len = strtoul(slen, NULL, 10);
    unsigned long crc = strtoul(scrc, NULL, 16);

    if (fseek(kernel, offset, SEEK_SET)) {
      perror("lseek");
      return 1;
    }

    if (fread(buffer, len, 1, kernel) < 1) {
      perror("read");
      return 1;
    }

    uint32_t calculated_crc = crc32(CRC32_INITIAL, buffer, len);
    printf("offset: %lu, len: %lu, ") assert(crc == calculated_crc);
  }
  return 0;
}

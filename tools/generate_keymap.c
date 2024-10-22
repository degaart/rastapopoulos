#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#define UTF8_IS4(c)    (((c)&0xF8) == 0xF0)
#define UTF8_IS3(c)    (((c)&0xF0) == 0xE0)
#define UTF8_IS2(c)    (((c)&0xE0) == 0xC0)
#define UTF8_IS1(c)    (((c)&0x80) == 0x00)
#define UTF8_ISCONT(c) (((c)&0xC0) == 0x80)

struct key {
    uint8_t scancode;
    char repr[4][16];
    struct key* next;
};

int main()
{
    char buffer[1024];
    int linenum = 0;
    struct key* keys = NULL;
    uint32_t count = 0;
    while(1) {
        char* ret = fgets(buffer, sizeof(buffer), stdin);
        if(!ret) {
            if(ferror(stdin)) {
                perror("read");
                return 1;
            } else {
                break;
            }
        }
        linenum++;

        size_t buffer_len = strlen(buffer);
        if(buffer_len) {
            while(buffer[buffer_len - 1] == '\n' ||
                  buffer[buffer_len - 1] == '\r') {
                buffer[buffer_len - 1] = '\0';
                buffer_len--;
            }

            char* token = strtok(buffer, "\t");
            if(!token) {
                fprintf(stderr, "Invalid format at line %d\n", linenum);
                return 1;
            }

            char* tokend;
            long lscancode = strtol(token, &tokend, 16);
            if(lscancode == 0 || lscancode == LONG_MAX ||
               lscancode == LONG_MIN) {
                fprintf(stderr, "Invalid format at line %d\n", linenum);
                return 1;
            } else if(lscancode < 0x00 || lscancode > 0xFF) {
                fprintf(stderr, "Invalid scancode at line %d\n", linenum);
                return 1;
            }

            struct key* key = calloc(1, sizeof(struct key));
            key->scancode = lscancode & 0xFF;

            for(int i = 0; i < 4; i++) {
                token = strtok(NULL, "\t");
                if(!token) {
                    fprintf(stderr, "Invalid format at line %d\n", linenum);
                    return 1;
                }
                if(!strcmp(token, "<cr>")) {
                    token = "\n";
                } else if(!strcmp(token, "<tab>")) {
                    token = "\t";
                }
                strlcpy(key->repr[i], token, sizeof(key->repr[i]));
            }
            key->next = keys;
            keys = key;
            count++;
        }
    }

    size_t w = fwrite(&count, sizeof(count), 1, stdout);
    if(w != 1) {
        perror("write");
        return 1;
    }

    for(const struct key* k = keys; k; k = k->next) {
        w = fwrite(&k->scancode, sizeof(k->scancode), 1, stdout);
        if(w != 1) {
            perror("write");
            return 1;
        }

        for(int i = 0; i < 4; i++) {
            if(UTF8_IS4(k->repr[i][0] & 0xF0)) {
                fprintf(stderr, "0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
                        (unsigned)k->scancode, (unsigned char)k->repr[i][0],
                        (unsigned char)k->repr[i][1],
                        (unsigned char)k->repr[i][2],
                        (unsigned char)k->repr[i][3]);
            } else if(UTF8_IS3(k->repr[i][0] & 0xE0)) {
                fprintf(stderr, "0x%02X 0x%02X 0x%02X 0x%02X\n",
                        (unsigned)k->scancode, (unsigned char)k->repr[i][0],
                        (unsigned char)k->repr[i][1],
                        (unsigned char)k->repr[i][2]);
            } else if(UTF8_IS2(k->repr[i][0] & 0xC0)) {
                fprintf(stderr, "0x%02X 0x%02X 0x%02X\n", (unsigned)k->scancode,
                        (unsigned char)k->repr[i][0],
                        (unsigned char)k->repr[i][1]);
            } else if(UTF8_IS1(k->repr[i][0] & 0x80)) {
                fprintf(stderr, "0x%02X 0x%02X\n", (unsigned)k->scancode,
                        (unsigned char)k->repr[i][0]);
            } else {
                fprintf(stderr,
                        "Invalid utf8 byte sequence for scancode 0x%02X\n",
                        (unsigned)k->scancode);
                return 1;
            }
            size_t len = strlen(k->repr[i]);
            w = fwrite(k->repr[i], len, 1, stdout);
            if(w != 1) {
                perror("write");
                return 1;
            }
        }
    }
    return 0;
}

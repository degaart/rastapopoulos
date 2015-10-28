#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define BUFFERLEN 4096

struct val_t {
    uint8_t val;
    unsigned count;
};

int valcomp(const void* arg0, const void* arg1) {
    struct val_t* v0 = (struct val_t*)arg0;
    struct val_t* v1 = (struct val_t*)arg1;
    return v1->count - v0->count;
}

int main() {
    struct val_t values[256];

    const int bufferlen = BUFFERLEN;
    char buffer[BUFFERLEN];
    int i;
    size_t nread;

    for (i=0; i<256; ++i) {
        values[i].val = i;
        values[i].count = 0;
    }

    do {
        nread = fread(buffer, 1, bufferlen, stdin);
        
        for (i = 0; i < nread; ++i)
            values[i].count++;
    } while (nread == bufferlen);

    qsort(values, 256, sizeof(struct val_t), valcomp);

    for (i=0; i<256; ++i){
        printf("0x%02X %u\n", values[i].val, values[i].count);
    }
    return 0;
}


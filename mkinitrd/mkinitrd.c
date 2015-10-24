#include <stdio.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#include "../kernel/initrd_header.h"

int write_file(FILE* outfile, FILE* infile) {
    char buf[512];
    ssize_t read_bytes, written_bytes;

    while(1) {
        read_bytes = fread(buf, 1, sizeof(buf), infile);
        if(read_bytes == 1) {
            fprintf(stderr, "Read error\n");
            return 0;
        } else if(read_bytes == 0) {
            break;    
        }

        written_bytes = fwrite(buf, 1, read_bytes, outfile);
        if(written_bytes != read_bytes) {
            fprintf(stderr, "Write error\n");
            return 0;
        }
    }
    return 1;
}

int add_file(FILE* outfile, const char* filename, int last) {
    struct stat st;
    if(stat(filename, &st) != 0) {
        fprintf(stderr, "%s: cannot stat\n", filename);
        return 0;
    }
    if( (st.st_mode & (S_IFREG & S_IFLNK)) == 0 ) {
        fprintf(stderr, "%s: Not a regular file or symlink\n", filename);
        return 0;
    } else if(st.st_size == 0) {
        fprintf(stderr, "Warning: %s is empty\n", filename);
    } else if(st.st_size >= UINT32_MAX) {
        fprintf(stderr, "%s: File too large\n", filename);
        return 0;
    }

    struct InitrdHeader_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.size = (uint32_t)st.st_size;
    hdr.last = last;

    char* buffer = strdup(filename);
    char* bname = basename(buffer);
    if(strlen(bname) >= sizeof(hdr.name)) {
        printf("%s: Filename too long\n", filename);
        return 0;
    }
    for(char* p = bname; *p; p++)
        *p = toupper(*p);

    strlcpy(hdr.name, bname, sizeof(hdr.name));
    free(buffer);

    if(fwrite(&hdr, sizeof(hdr), 1, outfile) != 1) {
        fprintf(stderr, "Write error while writing header\n");
        return 0;
    }

    FILE* infile = fopen(filename, "rb");
    if(!infile) {
        fprintf(stderr, "%s: cannot open\n", filename);
        return 0;
    }

    if(!write_file(outfile, infile)) {
        fclose(outfile);
        return 0;
    }

    fclose(infile);
    return 1;
}

int main(int argc, char** argv) {
    if(argc < 3) {
        fprintf(stderr, "Usage: %s <outfile> <infile>\n", argv[0]);
        return 1;
    }

    FILE* outfile = fopen(argv[1], "wb");
    if(!outfile) {
        fprintf(stderr, "Failed to open file %s\n", argv[1]);
        return 0;
    }

    for(int i=2; i<argc; i++) {
        //printf("%s\n", argv[i]);
        if(!add_file(outfile, argv[i], (i == argc - 1))) {
            return 1;
        }
    }

    return 0;
}


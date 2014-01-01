#include <stdio.h>
#include <stdlib.h>

#include <zlib.h>

#include "junzip.h"

int writeFile(char *filename, void *data, long bytes) {
    FILE *out = fopen(filename, "wb");

    if(out == NULL) {
        fprintf(stderr, "Couldn't open %s for writing!\n", filename);
        return -1;
    }

    fwrite(data, 1, bytes, out); // best effort is enough here

    fclose(out);

    return 0;
}

int readFile(FILE *zip) {
    JZFileHeader header;
    char filename[1024];
    unsigned char *data;

    if(jzReadLocalFileHeader(zip, &header, filename, sizeof(filename))) {
        printf("Couldn't read local file header!");
        return -1;
    }

    if((data = (unsigned char *)malloc(header.uncompressedSize)) == NULL) {
        printf("Couldn't allocate memory!");
        return -1;
    }

    printf("%s (%s), %ld / %ld bytes at offset %08lX\n", filename,
            jzMethods[header.compressionMethod],
            header.compressedSize, header.uncompressedSize, header.offset);

    if(jzReadData(zip, &header, data) != Z_OK) {
        printf("Couldn't read file data!");
        return -1;
    } else {
        writeFile(filename, data, header.uncompressedSize);
    }

    free(data);

    return 0;
}

int recordCallback(FILE *zip, int idx, JZFileHeader *header, char *filename) {
    long offset;

    printf("%s (%s), %ld / %ld bytes at offset %08lX\n", filename,
            jzMethods[header->compressionMethod],
            header->compressedSize, header->uncompressedSize, header->offset);

    offset = ftell(zip); // store position

    if(fseek(zip, header->offset, SEEK_SET)) {
        printf("Cannot seek in zip file!");
        return 0; // abort
    }

    readFile(zip);

    fseek(zip, offset, SEEK_SET); // return to position

    return 1; // continue
}

int main(int argc, char *argv[]) {
    FILE *zip;
    int retval = -1;
    JZEndRecord endRecord;

    if(argc < 2) {
        puts("Usage: ziplist file.zip");
        return -1;
    }

    if(!(zip = fopen(argv[1], "rb"))) {
        printf("Couldn't open \"%s\"!", argv[1]);
        return -1;
    }

    if(jzReadEndRecord(zip, &endRecord)) {
        printf("Couldn't read ZIP file end record.");
        goto endClose;
    }

    if(jzReadCentralDirectory(zip, &endRecord, recordCallback)) {
        printf("Couldn't read ZIP file central record.");
        goto endClose;
    }

    //while(!readFile(zip)) {}

    retval = 0;

endClose:
    fclose(zip);

    return retval;
}

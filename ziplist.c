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

void readFile(FILE *zip, JZFileHeader *fileHeader, char *filename) {
    long offset;
    JZFileHeader header;
    unsigned char *compressed, *uncompressed;
    z_stream infstream;
    int ret;

    offset = ftell(zip); // store position

    fseek(zip, fileHeader->offset, SEEK_SET);

    if(jzReadLocalFileHeader(zip, &header)) {
        printf("Couldn't read local file header!");
        return;
    }

    compressed = (unsigned char *)malloc(header.compressedSize);

    if(compressed == NULL) {
        puts("Couldn't allocate memory!");
        goto endRead;
    }

    // Read file in
    ret = fread(compressed, 1, header.compressedSize, zip);

    if(fileHeader->compressionMethod == 0) {
        uncompressed = compressed; // nothing to do here, move along
    } else if(fileHeader->compressionMethod == 8) { // deflate
        uncompressed = (unsigned char *)malloc(header.uncompressedSize);

        if(uncompressed == NULL) {
            puts("Couldn't allocate memory!");
            goto freeCompressed;
        }
        infstream.zalloc = Z_NULL;
        infstream.zfree = Z_NULL;
        infstream.opaque = Z_NULL;

        infstream.next_in = (Bytef *)compressed;
        infstream.avail_in = header.compressedSize;

        infstream.next_out = (Bytef *)uncompressed;
        infstream.avail_out = header.uncompressedSize;

        // Use inflateInit2 with negative windowbits to indicate raw deflate data
        if((ret = inflateInit2(&infstream, -MAX_WBITS)) != Z_OK) {
            printf("Zlib error %d while initializing.\n", ret);
            exit(1);
        }

        if((ret = inflate(&infstream, Z_NO_FLUSH)) < 0) {
            printf("Zlib error %d while inflating.\n", ret);
            exit(1);
        }

        inflateEnd(&infstream);
    } else {
        puts("Unsupported compression method!");
        goto freeCompressed;
    }

    writeFile(filename, uncompressed, header.uncompressedSize);

    if(uncompressed != compressed)
        free(uncompressed);

freeCompressed:
    free(compressed);

endRead:
    fseek(zip, offset, SEEK_SET);
}

int recordCallback(FILE *zip, int idx, JZFileHeader *header, char *filename) {
    printf("%s (%s), %ld / %ld bytes at offset %08lX\n", filename,
            jzMethods[header->compressionMethod],
            header->compressedSize, header->uncompressedSize, header->offset);
    readFile(zip, header, filename);

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

    retval = 0;

endClose:
    fclose(zip);

    return retval;
}

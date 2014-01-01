#include <stdio.h>
#include <stdlib.h>

#include <zlib.h>

typedef struct __attribute__ ((__packed__)) {
    unsigned long signature;
    unsigned short versionNeededToExtract;
    unsigned short generalPurposeBitFlag;
    unsigned short compressionMethod;
    unsigned short lastModFileTime;
    unsigned short lastModFileDate;
    unsigned long crc32;
    unsigned long compressedSize;
    unsigned long uncompressedSize;
    unsigned short fileNameLength;
    unsigned short extraFieldLength;
} ZipLocalFileHeader;

typedef struct __attribute__ ((__packed__)) {
    unsigned long signature;
    unsigned short versionMadeBy;
    unsigned short versionNeededToExtract;
    unsigned short generalPurposeBitFlag;
    unsigned short compressionMethod;
    unsigned short lastModFileTime;
    unsigned short lastModFileDate;
    unsigned long crc32;
    unsigned long compressedSize;
    unsigned long uncompressedSize;
    unsigned short fileNameLength;
    unsigned short extraFieldLength;
    unsigned short fileCommentLength;
    unsigned short diskNumberStart;
    unsigned short internalFileAttributes;
    unsigned long externalFileAttributes;
    unsigned long relativeOffsetOflocalHeader;
} ZipFileHeader;

typedef struct __attribute__ ((__packed__)) {
    unsigned long signature; // (0x06054b50)
    unsigned short diskNumber; // unsupported
    unsigned short centralDirectoryDiskNumber; // unsupported
    unsigned short numEntriesThisDisk; // unsupported
    unsigned short numEntries;
    unsigned long centralDirectorySize;
    unsigned long centralDirectoryOffset;
    unsigned short zipCommentLength;
    // Followed by .ZIP file comment (variable size)
} ZipEndOfCentralDirectoryRecord;

typedef struct __attribute__ ((__packed__)) {
    unsigned long headerSignature;
    unsigned short sizeOfData;
} ZipDigitalSignature;

// Simple memmem
unsigned char * findSignature(unsigned char *buffer, int bufsize, 
        unsigned char *signature, int sigsize) {
    int pos = 0;

    for(; bufsize > 0; bufsize--, buffer++) {
        if(*buffer == signature[pos]) {
            pos++;
            if(pos >= sigsize)
                return buffer - pos + 1;
        } else pos = 0;
    }

    return NULL;
}

unsigned char endSignature[4] = { 0x50, 0x4B, 0x05, 0x06 };
char zipMethods[13][16] = {
    "Store", "Shrunk", "Reduced #1", "Reduced #2", "Reduced #3", "Reduced #4",
    "Implode", "Reserved", "Deflate", "Deflate64", "PKImplode",
    "PKReserved", "BZIP2"
};

void readFile(FILE *zip, ZipFileHeader *fileHeader, char *filename) {
    long offset;
    ZipLocalFileHeader header;
    unsigned char *compressed, *uncompressed;
    FILE *out;
    z_stream infstream;
    int ret;

    offset = ftell(zip); // store position

    fseek(zip, fileHeader->relativeOffsetOflocalHeader, SEEK_SET);
    fread(&header, 1, sizeof(ZipLocalFileHeader), zip);

    if(header.signature != 0x04034B50) {
        printf("Invalid file header %08lX!", header.signature);
        goto endRead;
    }

    fseek(zip, header.fileNameLength, SEEK_CUR); // just skip...
    if(header.extraFieldLength)
        fseek(zip, header.extraFieldLength, SEEK_CUR); // just skip...

    if(header.generalPurposeBitFlag) {
        puts("Flags not supported!");
        goto endRead;
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

    out = fopen(filename, "wb");
    if(out == NULL) {
        printf("Couldn't open %s for writing!\n", filename);
    } else {
        fwrite(uncompressed, 1, header.uncompressedSize, out);
        fclose(out);
    }

    if(uncompressed != compressed)
        free(uncompressed);

freeCompressed:
    free(compressed);

endRead:
    fseek(zip, offset, SEEK_SET);
}

int main(int argc, char *argv[]) {
    FILE *zip;
    unsigned char buffer[4096]; // limits maximum zip descriptor size
    int retval = -1, i;
    long fileSize, totalSize = 0;
    ZipEndOfCentralDirectoryRecord *endRecord;
    ZipFileHeader fileHeader;
    char *strFilename, *strExtrafield, *strFilecomment;

    if(argc < 2) {
        puts("Usage: ziplist file.zip");
        return -1;
    }

    if(!(zip = fopen(argv[1], "rb"))) {
        puts("Couldn't open file!");
        return -1;
    }

    fseek(zip, 0, SEEK_END);
    fileSize = ftell(zip); // get file size

    if(fileSize < sizeof(buffer)) {
        puts("Too small file!");
        goto endClose;
    }

    fseek(zip, fileSize - sizeof(buffer), SEEK_SET);
    fread(buffer, 1, sizeof(buffer), zip);

    // Naively assume signature can only be found in one place...
    endRecord = (ZipEndOfCentralDirectoryRecord *)findSignature
        (buffer, sizeof(buffer), endSignature, 4);

    if(endRecord == NULL) {
        puts("End record not found!");
        goto endClose;
    }

    if(endRecord->diskNumber || endRecord->centralDirectoryDiskNumber ||
            endRecord->numEntries != endRecord->numEntriesThisDisk) {
        puts("Multifile zips not supported!");
        goto endClose;
    }

    printf("%d entries at %08lX (%ld bytes)\n", endRecord->numEntries,
            endRecord->centralDirectoryOffset,
            endRecord->centralDirectorySize);

    fseek(zip, endRecord->centralDirectoryOffset, SEEK_SET);

    for(i=0; i<endRecord->numEntries; i++) {
        fread(&fileHeader, 1, sizeof(ZipFileHeader), zip);

        if(fileHeader.signature != 0x02014B50) {
            puts("Invalid file header!");
            goto endClose;
        }

        strFilename = (char *)buffer;
        fread(strFilename, 1, fileHeader.fileNameLength, zip);
        strFilename[fileHeader.fileNameLength] = '\0'; // NULL terminate

        strExtrafield = strFilename + fileHeader.fileNameLength + 1;
        fread(strExtrafield, 1, fileHeader.extraFieldLength, zip);
        strExtrafield[fileHeader.extraFieldLength] = '\0'; // NULL terminate

        strFilecomment = strExtrafield + fileHeader.extraFieldLength + 1;
        fread(strFilecomment, 1, fileHeader.fileCommentLength, zip);
        strFilecomment[fileHeader.fileCommentLength] = '\0'; // NULL terminate

        printf("%s (%s)\n", strFilename,
                zipMethods[fileHeader.compressionMethod]);

        printf("  Size: %ld / %ld at offset %08lX\n", 
                fileHeader.compressedSize,
                fileHeader.uncompressedSize,
                fileHeader.relativeOffsetOflocalHeader);

        readFile(zip, &fileHeader, strFilename);

        totalSize += fileHeader.uncompressedSize;
    }

    printf("Total size of files %ld MB\n", totalSize / 1024 / 1024);

    retval = 0;

endClose:
    fclose(zip);

    return retval;
}

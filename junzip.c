#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "junzip.h"

unsigned char jzEndSignature[4] = { 0x50, 0x4B, 0x05, 0x06 };

char jzMethods[13][16] = {
    "Store", "Shrunk", "Reduced #1", "Reduced #2", "Reduced #3", "Reduced #4",
    "Implode", "Reserved", "Deflate", "Deflate64", "PKImplode",
    "PKReserved", "BZIP2"
};

unsigned char jzBuffer[JZ_BUFFER_SIZE]; // limits maximum zip descriptor size

// Simple memmem
unsigned char * jzFindSignature(unsigned char *jzBuffer, int bufsize,
        unsigned char *signature, int sigsize) {
    int pos = 0;

    for(; bufsize > 0; bufsize--, jzBuffer++) {
        if(*jzBuffer == signature[pos]) {
            pos++;
            if(pos >= sigsize)
                return jzBuffer - pos + 1;
        } else pos = 0;
    }

    return NULL;
}

// Read ZIP file end record. Will move within file.
int jzReadEndRecord(FILE *zip, JZEndRecord *endRecord) {
    unsigned char *signature;
    long fileSize, readBytes;

    if(fseek(zip, 0, SEEK_END)) {
        fprintf(stderr, "Couldn't go to end of zip file!");
        return -1;
    }

    if((fileSize = ftell(zip)) <= sizeof(JZEndRecord)) {
        fprintf(stderr, "Too small file to be a zip!");
        return -1;
    }

    readBytes = (fileSize < sizeof(jzBuffer)) ? fileSize : sizeof(jzBuffer);

    if(fseek(zip, fileSize - readBytes, SEEK_SET)) {
        fprintf(stderr, "Cannot seek in zip file!");
        return -1;
    }

    if(fread(jzBuffer, 1, readBytes, zip) < readBytes) {
        fprintf(stderr, "Couldn't read end of zip file!");
        return -1;
    }

    // Naively assume signature can only be found in one place...
    signature = jzFindSignature(jzBuffer, sizeof(jzBuffer), jzEndSignature, 4);

    if(signature == NULL) {
        fprintf(stderr, "End record signature not found in zip!");
        return -1;
    }

    memcpy(endRecord, signature, sizeof(JZEndRecord));

    if(endRecord->diskNumber || endRecord->centralDirectoryDiskNumber ||
            endRecord->numEntries != endRecord->numEntriesThisDisk) {
        fprintf(stderr, "Multifile zips not supported!");
        return -1;
    }

    return 0;
}

// Read ZIP file global directory. Will move within file.
int jzReadCentralDirectory(FILE *zip, JZEndRecord *endRecord,
        JZRecordCallback callback) {
    JZGlobalFileHeader fileHeader;
    JZFileHeader header;
    long totalSize = 0;
    int i;

    if(fseek(zip, endRecord->centralDirectoryOffset, SEEK_SET)) {
        fprintf(stderr, "Cannot seek in zip file!");
        return -1;
    }

    for(i=0; i<endRecord->numEntries; i++) {
        if(fread(&fileHeader, 1, sizeof(JZGlobalFileHeader), zip) <
                sizeof(JZGlobalFileHeader)) {
            fprintf(stderr, "Couldn't read file header %d!", i);
            return -1;
        }

        if(fileHeader.signature != 0x02014B50) {
            fprintf(stderr, "Invalid file header signature %d!", i);
            return -1;
        }

        if(fread(jzBuffer, 1, fileHeader.fileNameLength, zip) <
                fileHeader.fileNameLength) {
            fprintf(stderr, "Couldn't read filename %d!", i);
            return -1;
        }

        // I really don't believe there will be 65536-character filenames, but
        if(fileHeader.fileNameLength < JZ_BUFFER_SIZE)
            jzBuffer[fileHeader.fileNameLength] = '\0'; // NULL terminate
        else
            jzBuffer[JZ_BUFFER_SIZE - 1] = '\0'; // we'll miss one character :(

        if(fseek(zip, fileHeader.extraFieldLength, SEEK_CUR) ||
                fseek(zip, fileHeader.fileCommentLength, SEEK_CUR)) {
            fprintf(stderr, "Couldn't skip extra field or file comment %d", i);
            return -1;
        }

        // Construct JZFileHeader from global file header
        memcpy(&header, &fileHeader.compressionMethod, sizeof(header));
        header.offset = fileHeader.relativeOffsetOflocalHeader;

        totalSize += fileHeader.uncompressedSize;

        callback(zip, i, &header, (char *)jzBuffer);
    }

    return 0;
}

// Read local ZIP file header. Silent on errors so optimistic reading possible.
int jzReadLocalFileHeader(FILE *zip, JZFileHeader *header) {
    JZLocalFileHeader localHeader;

    if(fread(&localHeader, 1, sizeof(JZLocalFileHeader), zip) <
            sizeof(JZLocalFileHeader))
        return -1;

    if(localHeader.signature != 0x04034B50)
        return -1;

    if(fseek(zip, localHeader.fileNameLength, SEEK_CUR))
        return -1;

    if(localHeader.extraFieldLength) {
        if(fseek(zip, localHeader.extraFieldLength, SEEK_CUR))
            return -1;
    }

    if(localHeader.generalPurposeBitFlag)
        return -1; // Flags not supported

    memcpy(header, &localHeader, sizeof(JZFileHeader));
    header->offset = ftell(zip); // shouldn't fail, but might - who cares?

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

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

        if(!callback(zip, i, &header, (char *)jzBuffer))
            break; // end if callback returns zero
    }

    return 0;
}

// Read local ZIP file header. Silent on errors so optimistic reading possible.
int jzReadLocalFileHeader(FILE *zip, JZFileHeader *header,
        char *filename, int len) {
    JZLocalFileHeader localHeader;

    if(fread(&localHeader, 1, sizeof(JZLocalFileHeader), zip) <
            sizeof(JZLocalFileHeader))
        return -1;

    if(localHeader.signature != 0x04034B50)
        return -1;

    if(len) { // read filename
        if(localHeader.fileNameLength >= len)
            return -1; // filename cannot fit

        if(fread(filename, 1, localHeader.fileNameLength, zip) <
                localHeader.fileNameLength)
            return -1; // read fail

        filename[localHeader.fileNameLength] = '\0'; // NULL terminate
    } else { // skip filename
        if(fseek(zip, localHeader.fileNameLength, SEEK_CUR))
            return -1;
    }

    if(localHeader.extraFieldLength) {
        if(fseek(zip, localHeader.extraFieldLength, SEEK_CUR))
            return -1;
    }

    if(localHeader.generalPurposeBitFlag)
        return -1; // Flags not supported

    if(localHeader.compressionMethod == 0 &&
            (localHeader.compressedSize != localHeader.uncompressedSize))
        return -1; // Method is "store" but sizes indicate otherwise, abort

    memcpy(header, &localHeader.compressionMethod, sizeof(JZFileHeader));
    header->offset = 0; // not used in local context

    return 0;
}

// Read data from file stream, described by header, to preallocated buffer
// Return value is zlib coded, e.g. Z_OK, or error code
int jzReadData(FILE *zip, JZFileHeader *header, void *buffer) {
    unsigned char *bytes = (unsigned char *)buffer; // cast
    long compressedLeft, uncompressedLeft;
    z_stream strm;
    int ret;

    if(header->compressionMethod == 0) { // Store
        if(fread(buffer, 1, header->uncompressedSize, zip) <
                header->uncompressedSize || ferror(zip))
            return Z_ERRNO;
    } else if(header->compressionMethod == 8) { // Deflate
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;

        strm.avail_in = 0;
        strm.next_in = Z_NULL;

        // Use inflateInit2 with negative windowbits to indicate raw deflate data
        if((ret = inflateInit2(&strm, -MAX_WBITS)) != Z_OK)
            return ret; // Zlib errors are negative

        // Inflate compressed data
        for(compressedLeft = header->compressedSize,
                uncompressedLeft = header->uncompressedSize;
                compressedLeft && uncompressedLeft && ret != Z_STREAM_END;
                compressedLeft -= strm.avail_in) {
            // Read next chunk
            strm.avail_in = fread(jzBuffer, 1,
                    (sizeof(jzBuffer) < compressedLeft) ?
                    sizeof(jzBuffer) : compressedLeft, zip);

            if(strm.avail_in == 0 || ferror(zip)) {
                inflateEnd(&strm);
                return Z_ERRNO;
            }

            strm.next_in = jzBuffer;
            strm.avail_out = uncompressedLeft;
            strm.next_out = bytes;

            compressedLeft -= strm.avail_in; // inflate will change avail_in

            ret = inflate(&strm, Z_NO_FLUSH);

            if(ret == Z_STREAM_ERROR) return ret; // shouldn't happen

            switch (ret) {
                case Z_NEED_DICT:
                    ret = Z_DATA_ERROR;     /* and fall through */
                case Z_DATA_ERROR:
                case Z_MEM_ERROR:
                    (void)inflateEnd(&strm);
                    return ret;
            }

            bytes += uncompressedLeft - strm.avail_out; // bytes uncompressed
            uncompressedLeft = strm.avail_out;
        }

        inflateEnd(&strm);
    } else {
        return Z_ERRNO;
    }

    return Z_OK;
}

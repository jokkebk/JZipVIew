#ifndef __JUNZIP_H
#define __JUNZIP_H

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
} JZLocalFileHeader;

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
} JZGlobalFileHeader;

typedef struct __attribute__ ((__packed__)) {
    unsigned short compressionMethod;
    unsigned short lastModFileTime;
    unsigned short lastModFileDate;
    unsigned long crc32;
    unsigned long compressedSize;
    unsigned long uncompressedSize;
    unsigned long offset;
} JZFileHeader;

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
} JZEndRecord;

/*
typedef struct __attribute__ ((__packed__)) {
    unsigned long headerSignature;
    unsigned short sizeOfData;
} ZipDigitalSignature;
*/

// Callback prototype for central and local file record reading functions
typedef int (*JZRecordCallback)(FILE *zip, int index, JZFileHeader *header,
        char *filename);

#define JZ_BUFFER_SIZE 65536

extern unsigned char jzEndSignature[4];
extern char jzMethods[13][16];

// Read ZIP file end record. Will move within file.
int jzReadEndRecord(FILE *zip, JZEndRecord *endRecord);

// Read ZIP file global directory. Will move within file.
int jzReadCentralDirectory(FILE *zip, JZEndRecord *endRecord,
        JZRecordCallback callback);

// Read local ZIP file header. Silent on errors so optimistic reading possible.
int jzReadLocalFileHeader(FILE *zip, JZFileHeader *header,
        char *filename, int len);

// Read data from file stream, described by header, to preallocated buffer
// Return value is zlib coded, e.g. Z_OK, or error code
int jzReadData(FILE *zip, JZFileHeader *header, void *buffer);

#endif

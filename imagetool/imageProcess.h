#ifndef CTPKPROCESS_H
#define CTPKPROCESS_H

#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include <zstd.h>

// GL(EXT) definitions
#define GL_RGBA       0x1908

#define GL_RGB4_EXT   0x804F
#define GL_RGBA8_EXT  0x8058
#define GL_RGBA16_EXT 0x805B

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include "common.h"

#define RECOMPRESS_LVL 6

#define JPEG_QUALITY_LVL 95

#define IMAGE_MAGIC 0x69796F62 // "boyi"
#define IMAGE_VERSION 0x00000001
#define KTX_IDENTIFIER "\xAB\x4B\x54\x58\x20\x31\x31\xBB\x0D\x0A\x1A\x0A" // «KTX 11»....

#define KTX_LITTLE_ENDIAN 0x04030201
#define KTX_BIG_ENDIAN    0x01020304

// ImageFileHeader
typedef struct __attribute__((packed)) {
    u32 magic; // Compare to IMAGE_MAGIC
    u32 version;

    u16 width;
    u16 height;

    u16 _width;
    u16 _height;

    u32 compressedDataSize;
    u32 decompressedDataSize;

    u16 maskWidth;
    u16 maskHeight;

    u32 maskCompressedDataSize;
    u32 maskDecompressedDataSize;

    u8 headerEnd[0];
} ImageFileHeader;

// KTXLevel
typedef struct __attribute__((packed)) {
    u32 imageSize;
    u8 data[0];
} KTXLevel;

// KTXHeader
typedef struct __attribute__((packed)) {
    u8 identifier[12]; // Compare to KTX_IDENTIFIER

    u32 endianness; // Compare to KTX_BIG_ENDIAN or KTX_LITTLE_ENDIAN

    u32 glType;
    u32 glTypeSize;
    u32 glFormat;
    u32 glInternalFormat;
    u32 glBaseInternalFormat;

    u32 pixelWidth;
    u32 pixelHeight;
    u32 pixelDepth;

    u32 numberOfArrayElements;
    u32 numberOfFaces;
    u32 numberOfMipmapLevels;

    u32 bytesOfKeyValueData;

    u8 headerEnd[0];
} KTXHeader;

// Identifier check, endian processing, value correction
void KTXPreprocess(u8* ktxData) {
    KTXHeader* ktxHeader = (KTXHeader*)ktxData;
    if (memcmp(ktxHeader->identifier, KTX_IDENTIFIER, 12) != 0)
        panic("KTX header identifier is nonmatching");

    if (
        ktxHeader->endianness != KTX_LITTLE_ENDIAN &&
        ktxHeader->endianness != KTX_BIG_ENDIAN
    )
        panic("KTX header has bad endianness value");

    if (ktxHeader->endianness == KTX_BIG_ENDIAN) {
        ktxHeader->glType = __builtin_bswap32(ktxHeader->glType);
        ktxHeader->glTypeSize = __builtin_bswap32(ktxHeader->glTypeSize);
        ktxHeader->glFormat = __builtin_bswap32(ktxHeader->glFormat);
        ktxHeader->glInternalFormat = __builtin_bswap32(ktxHeader->glInternalFormat);
        ktxHeader->glBaseInternalFormat = __builtin_bswap32(ktxHeader->glBaseInternalFormat);

        ktxHeader->pixelWidth = __builtin_bswap32(ktxHeader->pixelWidth);
        ktxHeader->pixelHeight = __builtin_bswap32(ktxHeader->pixelHeight);
        ktxHeader->pixelDepth = __builtin_bswap32(ktxHeader->pixelDepth);

        ktxHeader->numberOfArrayElements = __builtin_bswap32(ktxHeader->numberOfArrayElements);
        ktxHeader->numberOfFaces = __builtin_bswap32(ktxHeader->numberOfFaces);
        ktxHeader->numberOfMipmapLevels = __builtin_bswap32(ktxHeader->numberOfMipmapLevels);

        ktxHeader->bytesOfKeyValueData = __builtin_bswap32(ktxHeader->bytesOfKeyValueData);

        // Process levels
        {
            u8* basePtr = ktxHeader->headerEnd + ktxHeader->bytesOfKeyValueData;
            u8* ptr = basePtr;
            for (unsigned i = 0; i < ktxHeader->numberOfMipmapLevels; i++) {
                ((KTXLevel*)ptr)->imageSize = __builtin_bswap32(((KTXLevel*)ptr)->imageSize);

                ptr += sizeof(KTXLevel) + ((KTXLevel*)ptr)->imageSize;

                u32 offset = ptr - basePtr;
                u32 padding = (4 - (offset % 4)) % 4;

                ptr += padding;
            }
        }

        ktxHeader->endianness = KTX_LITTLE_ENDIAN;
    }

    if (ktxHeader->numberOfMipmapLevels == 0)
        ktxHeader->numberOfMipmapLevels = 1;
    if (ktxHeader->numberOfArrayElements == 0)
        ktxHeader->numberOfArrayElements = 1;

    if (ktxHeader->pixelHeight == 0)
        ktxHeader->pixelHeight = 1;
    if (ktxHeader->pixelDepth == 0)
        ktxHeader->pixelDepth = 1;
}

u32 KTXGetGLFormat(u8* ktxData) {
    return ((KTXHeader*)ktxData)->glInternalFormat;
}

u32 KTXGetPixelComp(u8* ktxData) {
    switch (KTXGetGLFormat(ktxData)) {
    case GL_RGB4_EXT:
        return 3;
    case GL_RGBA8_EXT:
        return 4;
    case GL_RGBA16_EXT:
        return 8;
    
    default:
        return 4;
    }
}

u32 KTXGetLevelCount(u8* ktxData) {
    return ((KTXHeader*)ktxData)->numberOfMipmapLevels;
}

u32* KTXGetImageSize(u8* ktxData) {
    return &((KTXHeader*)ktxData)->pixelWidth;
}

KTXLevel* KTXGetLevelZero(u8* ktxData) {
    KTXHeader* ktxHeader = (KTXHeader*)ktxData;

    return (KTXLevel*)(ktxHeader->headerEnd + ktxHeader->bytesOfKeyValueData);
}

KTXLevel* KTXGetLevel(u8* ktxData, u32 mipIndex) {
    KTXHeader* ktxHeader = (KTXHeader*)ktxData;

    u8* basePtr = ktxHeader->headerEnd + ktxHeader->bytesOfKeyValueData;
    u8* ptr = basePtr;

    if (mipIndex >= ktxHeader->numberOfMipmapLevels)
        panic("Bad mipIndex: out of bounds");

    for (unsigned i = 0; i < mipIndex; i++) {
        ptr += sizeof(KTXLevel) + ((KTXLevel*)ptr)->imageSize;

        u32 offset = ptr - basePtr;
        u32 padding = (4 - (offset % 4)) % 4;

        ptr += padding;
    }

    return (KTXLevel*)ptr;
}

int ImageGetMaskExists(u8* imageData) {
    return ((ImageFileHeader*)imageData)->maskDecompressedDataSize != 0;
}

u16* ImageGetMaskSize(u8* imageData) {
    return &((ImageFileHeader*)imageData)->maskWidth;
}

// Must be freed after creation
u8* ImageCreateKTXData(u8* imageData) {
    ImageFileHeader* fileHeader = (ImageFileHeader*)imageData;
    if (fileHeader->magic != IMAGE_MAGIC)
        panic("Image header magic is nonmatching");

    if (memcmp((u32*)&fileHeader->width, (u32*)&fileHeader->_width, sizeof(u32)) != 0)
        panic("Image header sizes are nonmatching");

    printf("Alloc KTX decompress buffer (size : %u) ..", fileHeader->decompressedDataSize);

    u8* ktxData = (u8*)malloc(fileHeader->decompressedDataSize);
    if (ktxData == NULL)
        panic("Failed to allocate memory (KTX decompressed buffer)");

    LOG_OK;

    printf("Decompressing ..");

    u64 zstdResult = ZSTD_decompress(
        ktxData, fileHeader->decompressedDataSize,
        fileHeader->headerEnd, fileHeader->compressedDataSize
    );

    if (ZSTD_isError(zstdResult)) {
        free(ktxData);
        panic("Decompression error");
    }

    LOG_OK;

    KTXPreprocess(ktxData);

    return ktxData;
}

// A8 image data
// Must be freed after creation
u8* ImageCreateMaskData(u8* imageData) {
    ImageFileHeader* fileHeader = (ImageFileHeader*)imageData;
    if (fileHeader->magic != IMAGE_MAGIC)
        panic("Image header magic is nonmatching");

    /*
    if (memcmp((u32*)&fileHeader->width, (u32*)&fileHeader->_width, sizeof(u32)) != 0)
        panic("Image header sizes are nonmatching");
    */

    if (fileHeader->maskDecompressedDataSize == 0)
        panic("Mask does not exist");

    if (
        fileHeader->maskDecompressedDataSize !=
            fileHeader->maskWidth * fileHeader->maskHeight
    )
        panic("Unexpected mask data size");

    printf("Alloc mask decompress buffer (size : %u) ..", fileHeader->maskDecompressedDataSize);

    u8* maskData = (u8*)malloc(fileHeader->maskDecompressedDataSize);
    if (maskData == NULL)
        panic("Failed to allocate memory (mask decompressed buffer)");

    LOG_OK;

    printf("Decompressing ..");

    u64 zstdResult = ZSTD_decompress(
        maskData, fileHeader->maskDecompressedDataSize,
        fileHeader->headerEnd + fileHeader->compressedDataSize,
        fileHeader->maskCompressedDataSize
    );

    if (ZSTD_isError(zstdResult)) {
        free(maskData);
        panic("Decompression error");
    }

    LOG_OK;

    return maskData;
}

// Image data must be RGBA8
// Must be freed after creation
u8* KTXCreate(u8* imageData, u16 imageWidth, u16 imageHeight, u32* ktxSizeOut) {
    u64 dataSectionSize =
        sizeof(KTXLevel) +
        (imageWidth * imageHeight * 4);

    u16 mipmapsWidth = bitLength(imageWidth);
    u16 mipmapsHeight = bitLength(imageHeight);
    u32 mipCount = (mipmapsWidth < mipmapsHeight) ? mipmapsWidth : mipmapsHeight;

    // Skip levelzero
    for (unsigned i = 1; i < mipCount; i++) {
        float divBy = powf(2, i);
        u32 width = imageWidth / divBy;
        u32 height = imageHeight / divBy;

        dataSectionSize +=
            sizeof(KTXLevel) +
            (width * height * 4);

        // No padding needed since RGBA8 naturally rounds to 4.
    }

    u64 fullSize = sizeof(KTXHeader) + dataSectionSize;

    printf("Alloc KTX buffer (size : %lu) ..", fullSize);

    u8* ktxData = (u8*)malloc(fullSize);
    if (ktxData == NULL)
        panic("Failed to allocate memory (KTX buffer)");

    LOG_OK;

    KTXHeader* ktxHeader = (KTXHeader*)ktxData;

    memcpy(ktxHeader->identifier, KTX_IDENTIFIER, 12);

    ktxHeader->endianness = KTX_LITTLE_ENDIAN;

    ktxHeader->glType = 0;
    ktxHeader->glTypeSize = 1;
    ktxHeader->glFormat = 0;
    ktxHeader->glInternalFormat = GL_RGBA8_EXT;
    ktxHeader->glBaseInternalFormat = GL_RGBA;

    ktxHeader->pixelWidth = imageWidth;
    ktxHeader->pixelHeight = imageHeight;
    ktxHeader->pixelDepth = 1;

    ktxHeader->numberOfMipmapLevels = mipCount + 1; // Including levelzero

    ktxHeader->numberOfArrayElements = 1;
    ktxHeader->numberOfFaces = 0;

    ktxHeader->bytesOfKeyValueData = 0;

    KTXLevel* levelZero = (KTXLevel*)ktxHeader->headerEnd;

    levelZero->imageSize = imageWidth * imageHeight * 4;
    memcpy(levelZero->data, imageData, levelZero->imageSize);

    for (unsigned int i = 1; i < mipCount; i++) {
        KTXLevel* level = KTXGetLevel(ktxData, i);

        float divBy = powf(2, i);
        u32 newWidth = imageWidth / divBy;
        u32 newHeight = imageHeight / divBy;

        level->imageSize = newWidth * newHeight * 4;

        // Bilinear scaling
        {
            float xRatio = (float)(imageWidth - 1) / newWidth;
            float yRatio = (float)(imageHeight - 1) / newHeight;

            for (unsigned i = 0; i < newHeight; i++) {
                for (unsigned j = 0; j < newWidth; j++) {
                    unsigned x     = (unsigned)(xRatio * j);
                    unsigned y     = (unsigned)(yRatio * i);
                    float    xDiff = (xRatio * j) - x;
                    float    yDiff = (yRatio * i) - y;

                    unsigned index = (y * imageWidth + x) * 4;

                    for (unsigned c = 0; c < 4; c++) {
                        level->data[(i * newWidth + j) * 4 + c] = (u8)(
                            imageData[index + c] * (1 - xDiff) * (1 - yDiff) +
                            imageData[index + 4 + c] * xDiff * (1 - yDiff) +
                            imageData[(y + 1) * imageWidth * 4 + x * 4 + c] * (1 - xDiff) * yDiff +
                            imageData[(y + 1) * imageWidth * 4 + (x + 1) * 4 + c] * xDiff * yDiff
                        );
                    }
                }
            }
        }
    }

    if (ktxSizeOut != NULL)
        *ktxSizeOut = fullSize;

    return ktxData;
}

// Must be freed after creation
u8* ImageCreate(u8* ktxData, u32 ktxSize, u8* maskData, u16 maskWidth, u16 maskHeight, u32* imageSizeOut) {
    u8* ktxCompressedBuf;
    u64 ktxCompressedSize;

    u8* maskCompressedBuf = NULL;
    u64 maskCompressedSize = 0;

    // KTX compression
    {
        u64 maxCompressedSize = ZSTD_compressBound(ktxSize);

        printf("Alloc KTX compress buffer (size : %lu) ..", maxCompressedSize);
        ktxCompressedBuf = (u8*)malloc(maxCompressedSize);
        if (ktxCompressedBuf == NULL)
            panic("Failed to allocate memory (KTX compressed buffer)");

        LOG_OK;

        printf("Compressing KTX data ..");

        ktxCompressedSize = ZSTD_compress(
            ktxCompressedBuf, maxCompressedSize,
            ktxData, ktxSize,
            RECOMPRESS_LVL
        );

        if (ZSTD_isError(ktxCompressedSize)) {
            free(ktxCompressedBuf);
            panic("ZSTD compress failed");
        }

        LOG_OK;
    }

    if (maskData) {
        u64 maxCompressedSize = ZSTD_compressBound(maskWidth * maskHeight);

        printf("Alloc mask compress buffer (size : %lu) ..", maxCompressedSize);
        maskCompressedBuf = (u8*)malloc(maxCompressedSize);
        if (maskCompressedBuf == NULL)
            panic("Failed to allocate memory (mask compressed buffer)");

        LOG_OK;

        printf("Compressing mask data ..");

        maskCompressedSize = ZSTD_compress(
            maskCompressedBuf, maxCompressedSize,
            maskData, maskWidth * maskHeight,
            RECOMPRESS_LVL
        );

        if (ZSTD_isError(maskCompressedSize)) {
            free(maskCompressedBuf);
            panic("ZSTD compress failed");
        }

        LOG_OK;
    }

    u64 fullSize = sizeof(ImageFileHeader) + ktxCompressedSize + maskCompressedSize;

    printf("Alloc image binary buffer (size : %lu) ..", fullSize);

    u8* imageData = (u8*)malloc(fullSize);
    if (imageData == NULL)
        panic("Failed to allocate memory (image binary buffer)");

    LOG_OK;

    ImageFileHeader* fileHeader = (ImageFileHeader*)imageData;

    printf("Copying compressed data ..");

    memcpy(fileHeader->headerEnd, ktxCompressedBuf, ktxCompressedSize);
    free(ktxCompressedBuf);

    if (maskCompressedBuf) {
        memcpy(
            fileHeader->headerEnd + ktxCompressedSize,
            maskCompressedBuf, maskCompressedSize
        );
        free(maskCompressedBuf);
    }

    LOG_OK;

    fileHeader->magic = IMAGE_MAGIC;

    fileHeader->version = IMAGE_VERSION;

    fileHeader->compressedDataSize = ktxCompressedSize;
    fileHeader->decompressedDataSize = ktxSize;

    KTXHeader* ktxHeader = (KTXHeader*)ktxData;

    fileHeader->width = ktxHeader->pixelWidth;
    fileHeader->_width = ktxHeader->pixelWidth;
    fileHeader->height = ktxHeader->pixelHeight;
    fileHeader->_height = ktxHeader->pixelHeight;

    fileHeader->maskWidth = maskWidth;
    fileHeader->maskHeight = maskHeight;
    fileHeader->maskCompressedDataSize = maskCompressedSize;
    fileHeader->maskDecompressedDataSize = maskWidth * maskHeight;

    if (imageSizeOut != NULL)
        *imageSizeOut = fullSize;

    return imageData;
}

void ImageExportTexture(u8* imageData, char* outputPath) {
    u8* ktxData = ImageCreateKTXData(imageData);

    printf("Writing images: \n");

    char* fileExtension = getFileExtension(outputPath);
    u32* imageSize = KTXGetImageSize(ktxData);
    u32 pixelComp = KTXGetPixelComp(ktxData);

    for (unsigned i = 0; i < KTXGetLevelCount(ktxData); i++) {
        KTXLevel* level = KTXGetLevel(ktxData, i);

        char fn[128];
        sprintf(
            fn, "%.*s.mip%u.%s",

            (int)(strlen(outputPath) - strlen(fileExtension) - 1),
            outputPath,

            i+1,
            fileExtension
        );

        printf(INDENT_SPACE "- Writing level no. %u to path '%s'..", i+1, fn);

        int mipWidth = imageSize[0] / pow(2, i);
        int mipHeight = imageSize[1] / pow(2, i);

        if (mipWidth <= 0 || mipHeight <= 0) {
            printf(" Skipped (too small)\n");
            continue;
        }

        int writeResult = 0;

        if (strcmp(fileExtension, "bmp") == 0) {
            writeResult = stbi_write_bmp(
                fn,
                mipWidth, mipHeight,
                pixelComp, level->data
            );
        } else if (strcmp(fileExtension, "jpg") == 0) {
            writeResult = stbi_write_jpg(
                fn,
                mipWidth, mipHeight,
                pixelComp, level->data,
                JPEG_QUALITY_LVL
            );
        } else if (strcmp(fileExtension, "tga") == 0) {
            writeResult = stbi_write_tga(
                fn,
                mipWidth, mipHeight,
                pixelComp, level->data
            );
        } else { // Default is PNG
            writeResult = stbi_write_png(
                fn,
                mipWidth, mipHeight,
                pixelComp, level->data,
                4 * mipWidth
            );
        }

        if (writeResult == 0)
            panic("The output image could not be created.");

        LOG_OK;
    }

    if (ImageGetMaskExists(imageData)) {
        u8* maskData = ImageCreateMaskData(imageData);
        u16* maskSize = ImageGetMaskSize(imageData);

        char fn[128];
        sprintf(
            fn, "%.*s.mask.png",

            (int)(strlen(outputPath) - strlen(fileExtension) - 1),
            outputPath
        );


        printf("Writing mask data to path '%s'..", fn);

        int writeResult = stbi_write_png(
            fn,
            maskSize[0], maskSize[1],
            1, maskData,
            1 * maskSize[0]
        );
        
        if (writeResult == 0)
            panic("The mask data could not be exported.");

        LOG_OK;
    }

    printf("Extraction finished.\n");

    free(ktxData);
}

#endif
#include <stdio.h>
#include <stdlib.h>

#include "imageProcess.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include "common.h"

void usage(int showTitle) {
    if (showTitle) {
        printf("Image Tool v1.0\n");
        printf("A utility for extracting or creating texture files for World of Goo 2.\n\n");
    }

    printf("Usage:\n");
    printf("    imagetool -e <input_image_file> -o <output_image_file>\n");
    printf("    imagetool -c <input_image_file> -o <output_image_file> [-m <mask_image_file>]\n\n");

    printf("Options:\n");
    printf("    -e, --extract        Extract textures from a .image file.\n");
    printf("                         <input_image_file>: Path to the .image file.\n");
    printf("                         <output_image_file>: Path for the extracted image with desired format (.png, .bmp, .tga, .jpg).\n\n");

    printf("    -c, --create         Create a .image file from an input image.\n");
    printf("                         <input_image_file>: Path to the source image (.png, .bmp, .tga, .psd, .jpg).\n");
    printf("                         <output_image_file>: Path for the created .image file.\n\n");

    printf("    -o, --output <path>  Specify the output path (required).\n\n");

    printf("    -m, --mask <path>    Optional: Specify a mask image when creating a .image file.\n");
    printf("                         Supported formats: .png, .bmp, .tga, .psd, .jpg.\n");
    printf("                         The mask image should use luminance (black = 0, white = 1).\n\n");

    printf("    -h, --help           Display this help message and exit.\n\n");

    printf("Examples:\n");
    printf("    Extract:           imagetool -e ./sample.image -o ./sample.png\n");
    printf("    Create:            imagetool -c ./sample.png -o ./sample.image\n");
    printf("    Create with mask:  imagetool -c ./sample.png -o ./sample.image -m ./sample_mask.png\n");
    printf("    Show help:         imagetool --help\n");

    exit(1);
}

#define COMMAND_BAD     0
#define COMMAND_EXTRACT 1
#define COMMAND_CREATE  2

int main(int argc, char* argv[]) {
    char* inputPath = NULL;
    char* outputPath = NULL;
    char* maskPath = NULL;
    
    unsigned command = COMMAND_BAD;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
            usage(1);
        else if (strcmp(argv[i], "--extract") == 0 || strcmp(argv[i], "-e") == 0)
            command = COMMAND_EXTRACT;
        else if (strcmp(argv[i], "--create") == 0 || strcmp(argv[i], "-c") == 0)
            command = COMMAND_CREATE;
        else if (strcmp(argv[i], "--output") == 0 || strcmp(argv[i], "-o") == 0) {
            if (i+1 < argc)
                outputPath = argv[++i];
            else {
                printf("Error: Missing output path after '%s'.\n\n", argv[i]);
                usage(0);
            }
        }
        else if (strcmp(argv[i], "--mask") == 0 || strcmp(argv[i], "-m") == 0) {
            if (i+1 < argc)
                maskPath = argv[++i];
            else {
                printf("Error: Missing mask path after '%s'.\n\n", argv[i]);
                usage(0);
            }
        }
        else
            inputPath = argv[i];
    }

    if (outputPath == NULL) {
        printf("Error: Missing output path.\n\n");
        usage(0);
    }

    if (command == COMMAND_BAD || inputPath == NULL) {
        printf("Error: Missing command or input path.\n\n");
        usage(0);
    }

    switch (command) {
    case COMMAND_EXTRACT: {
        if (maskPath != NULL)
            warn("A mask has been provided in extract mode. The mask will not be used.");

        printf("Read & copy image binary ..");

        FILE* fpImage = fopen(inputPath, "rb");
        if (fpImage == NULL)
            panic("The input image binary could not be opened.");

        fseek(fpImage, 0, SEEK_END);
        u64 imageSize = ftell(fpImage);
        rewind(fpImage);

        if (imageSize == 0) {
            fclose(fpImage);

            panic("The image binary is empty.");
        }

        u8* imageBuf = (u8 *)malloc(imageSize);
        if (imageBuf == NULL) {
            fclose(fpImage);

            panic("Failed to allocate memory (image binary buffer)");
        }

        u64 bytesCopied = fread(imageBuf, 1, imageSize, fpImage);
        if (bytesCopied != imageSize) {
            free(imageBuf);
            fclose(fpImage);

            panic("The input image binary could not be read.");
        }

        fclose(fpImage);

        LOG_OK;

        ImageExportTexture(imageBuf, outputPath);
    } break;

    case COMMAND_CREATE: {
        printf("Image file read-in ..");

        int imageWidth;
        int imageHeight;

        u8* inputData = stbi_load(inputPath, &imageWidth, &imageHeight, NULL, 4);
        if (inputData == NULL)
            panic("The input image file could not be opened.");

        LOG_OK;

        int maskWidth = 0;
        int maskHeight = 0;
        u8* maskData = NULL;

        if (maskPath) {
            maskData = stbi_load(maskPath, &maskWidth, &maskHeight, NULL, 1);
            if (maskData == NULL)
                panic("The input mask image could not be opened.");
        }

        u32 ktxSize;
        u8* ktxData = KTXCreate(inputData, imageWidth, imageHeight, &ktxSize);

        u32 imageSize;
        u8* imageData = ImageCreate(
            ktxData, ktxSize,
            maskData, (u16)maskWidth, (u16)maskHeight,
            &imageSize
        );

        printf("Write IMAGE to file ..");

        FILE* file = fopen(outputPath, "wb");
        if (file == NULL)
            panic("The output image binary could not be opened for writing. Does the directory exist?");

        fwrite(imageData, 1, imageSize, file);

        fclose(file);

        LOG_OK;

        free(ktxData);
        free(imageData);

        stbi_image_free(inputData);
        if (maskData)
            stbi_image_free(maskData);
    } break;
    
    default:
        panic("Invalid command");
        break;
    }

    printf("\nFinished! Exiting ..\n");

    return 0;
}
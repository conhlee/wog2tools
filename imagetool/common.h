#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>

#include <ctype.h>

#include <string.h>

typedef unsigned long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef signed long s64;
typedef signed int s32;
typedef signed short s16;
typedef signed char s8;

#define TRUE 1
#define FALSE 0

#define INDENT_SPACE "    "

#define LOG_OK printf(" OK\n")

void panic(const char* msg) {
    printf("\nPANIC: %s\nExiting ..\n", msg);
    exit(1);
}

void warn(const char* msg) {
    printf("\nWARN: %s\n", msg);
}

u16 bitLength(u16 value) {
    u16 length = 0;
    while (value > 0) {
        value >>= 1;
        length++;
    }
    
    return length;
}

char* getFilename(char* path) {
    char* lastSlash = strrchr(path, '/');
    if (!lastSlash)
        lastSlash = strrchr(path, '\\');

    if (!lastSlash)
        return path;

    return (char*)(lastSlash + 1);
}

char* getFileExtension(char* filename) {
    char* dot = strrchr(filename, '.');
    if (!dot || dot == filename)
        return "";

    char* extension = dot + 1;
    for (char* c = extension; *c; c++)
        *c = tolower(*c);

    return extension;
}

#endif
#ifndef LISTSUTILS_H
#define LISTSUTILS_H

#include <stdio.h>
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

// Define a structure for a JSON layer
typedef struct Layer {
    char digest[128];
    struct Layer *next;
} Layer;

typedef struct Manifest_parsed_info {
    char schemaVersion[128];
    char mediaType[128];
    char configMediaType[128];
    char configSize[128];
    char configDigest[128];
    Layer* layersList;
} Manifest_parsed_info;

// Define a struct to store digest and architecture
typedef struct ImageInfo {
    char digest[256];
    char architecture[64];
    struct ImageInfo* next;
} ImageInfo;

void printLayerDigests(Layer *layerList);
void freeLayerList(Layer *layerList);
ImageInfo* createImageInfo(const char* digest, const char* architecture);
void appendImageInfo(ImageInfo** head, ImageInfo* newNode);
void freeImageInfoList(ImageInfo* head);
void printImageInfoList(ImageInfo* head);
#endif
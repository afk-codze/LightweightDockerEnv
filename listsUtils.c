#include "listsUtils.h"

// Function to print the layer digests
void printLayerDigests(Layer *layerList) {
    Layer *currentLayer = layerList;
    int layerCount = 1;
    while (currentLayer != NULL) {
        printf("Layer %d Digest: %s\n", layerCount, currentLayer->digest);
        currentLayer = currentLayer->next;
        layerCount++;
    }
}

// Function to free the memory allocated for the linked list
void freeLayerList(Layer *layerList) {
    Layer *currentLayer = layerList;
    while (currentLayer != NULL) {
        Layer *nextLayer = currentLayer->next;
        free(currentLayer);
        currentLayer = nextLayer;
    }
}

// Function to create a new ImageInfo node
ImageInfo* createImageInfo(const char* digest, const char* architecture) {
    ImageInfo* newNode = (ImageInfo*)malloc(sizeof(ImageInfo));
    if (newNode) {
        strncpy(newNode->digest, digest, sizeof(newNode->digest) - 1);
        newNode->digest[sizeof(newNode->digest) - 1] = '\0';
        strncpy(newNode->architecture, architecture, sizeof(newNode->architecture) - 1);
        newNode->architecture[sizeof(newNode->architecture) - 1] = '\0';
        newNode->next = NULL;
    }
    return newNode;
}

// Function to add a new ImageInfo node to the linked list
void appendImageInfo(ImageInfo** head, ImageInfo* newNode) {
    if (*head == NULL) {
        *head = newNode;
    } else {
        ImageInfo* current = *head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = newNode;
    }
}

// Function to free the linked list
void freeImageInfoList(ImageInfo* head) {
    while (head != NULL) {
        ImageInfo* temp = head;
        head = head->next;
        free(temp);
    }
}

// Function to print the linked list
void printImageInfoList(ImageInfo* head) {
    ImageInfo* current = head;
    while (current != NULL) {
        printf("Digest: %s, Architecture: %s\n", current->digest, current->architecture);
        current = current->next;
    }
}
#include <ctype.h>
#include "networking.h"
#include "listsUtils.h"

static const char* extractStringValue(const char *json, const char *key, char *value, int maxLen) {
    const char *start = strstr(json, key);
    if (!start) {
        return NULL; // Key not found
    }

    start = strchr(start, ':');
    if (!start) {
        return NULL; // Malformed JSON
    }
    
    start++; // Move past the ':' character

    // Skip whitespace characters
    while (isspace(*start)) {
        start++;
    }

    if (*start != '"') {
        return NULL; // Not a string value
    }

    start++; // Move past the opening double quote

    int len = 0;
    while (*start != '"' && *start != '\0' && len < maxLen - 1) {
        value[len++] = *start++;
    }

    value[len] = '\0'; // Null-terminate the string

    return start; // Return the position after the closing double quote
}

Manifest_parsed_info* parse_manifest(const char *json_data) {
    Manifest_parsed_info* manifest_info = (Manifest_parsed_info*)malloc(sizeof(Manifest_parsed_info));
    if (!manifest_info) {
        return NULL;  // Memory allocation failure
    }
    
    // Initialize fields
    memset(manifest_info->mediaType, 0, sizeof(manifest_info->mediaType));
    memset(manifest_info->configDigest, 0, sizeof(manifest_info->configDigest));
    manifest_info->layersList = NULL;

    // Extract the values
    extractStringValue(json_data, "\"mediaType\":", manifest_info->mediaType, sizeof(manifest_info->mediaType));
    extractStringValue(json_data, "\"digest\":", manifest_info->configDigest, sizeof(manifest_info->configDigest));
    
    const char *layerStart = strstr(json_data, "\"layers\":");
    Layer *currentLayer = NULL;

    if (layerStart) {
        layerStart = strchr(layerStart, '[');
        while (layerStart) {
            char layerDigest[128];
            layerStart = extractStringValue(layerStart, "\"digest\":", layerDigest, sizeof(layerDigest));
            if (!layerStart) {
                break; // No more layers or an error occurred
            }

            Layer *newLayer = (Layer *)malloc(sizeof(Layer));
            if (!newLayer) {
                freeLayerList(manifest_info->layersList);
                free(manifest_info);
                return NULL;  // Memory allocation failure
            }

            strcpy(newLayer->digest, layerDigest);
            newLayer->next = NULL;
            
            if (!currentLayer) {
                manifest_info->layersList = newLayer;
            } else {
                currentLayer->next = newLayer;
            }

            currentLayer = newLayer;
        }
    }

    if (!manifest_info->layersList) {
        free(manifest_info);
        return NULL; // Empty list or an error occurred
    }

    return manifest_info;
}

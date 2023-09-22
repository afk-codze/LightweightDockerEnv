#ifndef PARSEMANIFEST_H
#define PARSEMANIFEST_H

#include "networking.h"
#include "listsUtils.h"

int extractStringValue(const char *json, const char *key, char *value, int maxLen);
Manifest_parsed_info* parse_manifest(const char *json_data); 
#endif
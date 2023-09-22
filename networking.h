#ifndef NETWORKING_H
#define NETWORKING_H

#include <curl/curl.h>
#include "listsUtils.h"

void initialize_curl_global(); 
void cleanup_curl_global();
FILE *open_unique_file(const char *basename, const char *ext);
size_t write_data_callback_file(void *contents, size_t size, size_t nmemb, void *userp);
void download_file(const char *url, const char *filename, const char *token);
ImageInfo *getManifestListElem(const char *json_data);
char * parse_token(char * raw_token);
char *get_auth_token(const char *image_name);
bool isImageManifest(const char *json_data);
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp);
char *get_response(const char *url, const char *token, const char *purpose);
char *get_manifest(const char *image_name, const char *token);
int move_file_to_directory(const char *filename, const char *dir_name);
int get_image(char *image_name, char *dir_name);
int untar_and_remove(const char * filename);
void clean_resources(char *manifest, Manifest_parsed_info *manifest_info);
#endif
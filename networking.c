/// sudo apt-get install libcurl4-openssl-dev

#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include "networking.h"
#include "listsUtils.h"
#include "parseManifest.h"

#define AUTH_PREFIX "Authorization: Bearer "
#define MAX_FILENAME_SIZE 256
#define DIGEST_PREFIX "\"digest\":\""
#define ARCHITECTURE_PREFIX "\"architecture\":\""
#define TOKEN_PREFIX "\"token\":"
#define AUTH_URL "https://auth.docker.io/token?service=registry.docker.io&scope=repository:"
#define ACTION ":pull"
#define ACCEPT_HEADER "Accept: application/vnd.docker.distribution.manifest.v2+json, application/vnd.oci.image.manifest.v1+json"

typedef struct {
    char *data;       // Pointer to our dynamic buffer
    size_t size;     // Current size of the buffer
} ResponseBuffer;

void initialize_curl_global() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void cleanup_curl_global() {
    curl_global_cleanup();
}

FILE *open_unique_file(const char *basename, const char *ext) {
    char filename[MAX_FILENAME_SIZE];
    int counter = 0;
    FILE *fp = NULL;

    do {
        snprintf(filename, sizeof(filename), "%s_%d%s", basename, counter, ext);
        fp = fopen(filename, "r");
        if (fp != NULL) {
            fclose(fp);
            counter++;
        }
    } while (fp != NULL);

    return fopen(filename, "w");
}

// Write callback to save the content to a file
size_t write_data_callback_file(void *contents, size_t size, size_t nmemb, void *userp) {
    return fwrite(contents, size, nmemb, (FILE *)userp);
}

void download_file(const char *url, const char *filename, const char *token) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize libcurl\n");
        return;
    }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/vnd.docker.distribution.manifest.v2+json, application/vnd.oci.image.manifest.v1+json");
    
    if (token) {
        char auth_header[strlen(token) + sizeof(AUTH_PREFIX)];
        snprintf(auth_header, sizeof(auth_header), AUTH_PREFIX "%s", token);
        headers = curl_slist_append(headers, auth_header);
    }

    FILE *fp = open_unique_file(filename, ".tar");
    if (!fp) {
        perror("Failed to open file for writing");
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        return;
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_callback_file);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    } else {
        long http_response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_response_code);
        if (http_response_code == 200) {
            printf("[+] File downloaded successfully.\n");
        } else {
            fprintf(stderr, "[-] HTTP request failed with status code: %ld\n", http_response_code);
        }
    }

    fclose(fp);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
}

// Function to get the manifest list elements and return the linked list
ImageInfo *getManifestListElem(const char *json_data) {
    ImageInfo *head = NULL;

    const char *start = strstr(json_data, DIGEST_PREFIX);
    while (start) {
        const char *end = strstr(start, "\"}");
        if (!end) break;
        end += 2;

        const char *digest_start = start + strlen(DIGEST_PREFIX);
        const char *digest_end = strstr(digest_start, "\",");
        const char *architecture_start = strstr(start, ARCHITECTURE_PREFIX) + strlen(ARCHITECTURE_PREFIX);
        const char *architecture_end = strstr(architecture_start, "\"");

        if (digest_start && digest_end && architecture_start && architecture_end) {
            char digest[256];
            char architecture[64];
            strncpy(digest, digest_start, digest_end - digest_start);
            digest[digest_end - digest_start] = '\0';
            strncpy(architecture, architecture_start, architecture_end - architecture_start);
            architecture[architecture_end - architecture_start] = '\0';

            ImageInfo *newNode = createImageInfo(digest, architecture);
            appendImageInfo(&head, newNode);
        }

        start = strstr(end, DIGEST_PREFIX);
    }
    
    return head;
}

char * parse_token(char * raw_token) {
  char * str = raw_token + 10;
  str = strtok(str, ",");
  int len = strlen(str);
  str[len - 1] = '\0';
  char * token = (char * ) malloc(len + 1);
  strcpy(token, str);
  return token;
}

char *get_auth_token(const char *image_name) {
    size_t auth_url_len = strlen(AUTH_URL) + strlen(image_name) + strlen(ACTION) + 1;
    char *final_auth_url = (char *)malloc(auth_url_len);

    if (!final_auth_url) {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }

    snprintf(final_auth_url, auth_url_len, "%s%s%s", AUTH_URL, image_name, ACTION);
    char *content = get_response(final_auth_url, NULL, "token");
    free(final_auth_url);

    if (!content) {
        fprintf(stderr, "Failed to obtain authentication token\n");
        return NULL;
    }

    char *token = parse_token(content);
    free(content);

    if (!token) {
        fprintf(stderr, "Failed to parse authentication token\n");
        return NULL;
    }

    return token;
}

bool isImageManifest(const char *json_data) {
    return strstr(json_data, "\"config\":") && strstr(json_data, "\"layers\":");
}

// Callback function to capture the HTTP response data
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    ResponseBuffer *buf = (ResponseBuffer *)userp;

    char *ptr = realloc(buf->data, buf->size + realsize + 1);
    if (!ptr) return 0; // Memory allocation failed

    buf->data = ptr;
    memcpy(&(buf->data[buf->size]), contents, realsize);
    buf->size += realsize;
    buf->data[buf->size] = '\0';

    return realsize;
}

char *get_response(const char *url, const char *token, const char *purpose) {
    CURL *curl;
    CURLcode res;

    // Initialize the dynamic response buffer
    ResponseBuffer buffer = {
        .data = malloc(1), // Starting with 1 byte to ease realloc
        .size = 0
    };

    if (!buffer.data) {
        fprintf(stderr, "Failed to initialize response buffer\n");
        return NULL;
    }
    buffer.data[0] = '\0';  // Null-terminate to begin with

    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize libcurl\n");
        free(buffer.data);
        return NULL;
    }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, ACCEPT_HEADER);

    // Add the "Authorization" header with token
    if (token) {
        char auth_header[strlen(token) + strlen(AUTH_PREFIX) + 1];
        snprintf(auth_header, sizeof(auth_header), "%s%s", AUTH_PREFIX, token);
        headers = curl_slist_append(headers, auth_header);
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

    res = curl_easy_perform(curl);  // Execute the GET request
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        free(buffer.data);
        buffer.data = NULL;
    } else {
        long http_response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_response_code);

        switch(http_response_code) {
            case 200:
                if (strcmp(purpose, "token") == 0) {
                    printf("[+] Authentication token fetched successfully.\n");
                } else if (strcmp(purpose, "image manifest") == 0) {
                    printf("[+] Image manifest fetched successfully.\n");
                } else if (strncmp(purpose, "Layer", 5) == 0) {
                    printf("[+] Layer fetched successfully.\n");
                } else {
                    printf("[+] Data fetched successfully.\n");
                }
                break;
            case 307:
                printf("Redirection required.\n");
                char *location = NULL;
                curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &location);
                if (location) {
                    printf("Redirecting to %s\n", location);
                    download_file(location, "downloaded_file", token);
                } else {
                    fprintf(stderr, "Redirection URL not found.\n");
                    free(buffer.data);
                    buffer.data = NULL;
                }
                break;
            default:
                fprintf(stderr, "HTTP request failed with status code: %ld\n", http_response_code);
                free(buffer.data);
                buffer.data = NULL;
        }
    }

    // Cleanup
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    return buffer.data;
}

char *get_manifest(const char *image_name, const char *token) {
    const char *registry_url = "https://registry.hub.docker.com";
    const char *image_reference = "latest";
    char manifest_url[1024];

    // Build the manifest URL
    snprintf(manifest_url, sizeof(manifest_url), "%s/v2/%s/manifests/%s", registry_url, image_name, image_reference);
    char *content = get_response(manifest_url, token, "image manifest");
    if (!content) {
        fprintf(stderr, "Failed to retrieve content\n");
        return NULL;
    }

    char *manifest = NULL;

    // Check if the content is an image manifest or a manifest list
    if (!isImageManifest(content)) {
        ImageInfo *manifestList = getManifestListElem(content);

        // Example: retrieve a specific image manifest at index 0
        int specificIndex = 0;
        ImageInfo *current = manifestList;
        for (int currentIndex = 0; current && currentIndex < specificIndex; currentIndex++) {
            current = current->next;
        }

        if (current) {
            char specific_manifest_url[1024];
            snprintf(specific_manifest_url, sizeof(specific_manifest_url), "%s/v2/%s/manifests/%s", registry_url, image_name, current->digest);
            manifest = get_response(specific_manifest_url, token, "image manifest");
        } else {
            fprintf(stderr, "Error: Index %d is out of bounds\n", specificIndex);
        }

        freeImageInfoList(manifestList);
    } else {
        manifest = content;
        content = NULL;  // To avoid double freeing
    }

    if (content) {
        free(content);
    }

    return manifest;
}

int move_file_to_directory(const char *filename, const char *dir_name) {
    char new_path[512];
    snprintf(new_path, sizeof(new_path), "%s/%s", dir_name, filename);

    if (rename(filename, new_path) == -1) {
        perror("Error moving file");
        return -1;
    }
    return 0;
}

int get_image(char *image_name, char *dir_name) {
    // Initialization
    initialize_curl_global();

    if (!chdir(dir_name) == 0) {
        perror("Error changing directory");
        cleanup_curl_global();
        return -1;
    }

    // Header print
    printf("\n============================================\n");
    printf("      Docker Image Fetch Utility\n");
    printf("============================================\n\n");

    // Get authentication token
    printf("[*] Fetching authentication token...\n");
    char *token = get_auth_token(image_name);
    if (!token) {
        fprintf(stderr, "Error retrieving authentication token.\n");
        cleanup_curl_global();
        return -1;
    }

    // Retrieve image manifest
    printf("\n[*] Retrieving image manifest for: %s...\n", image_name);
    char *manifest = get_manifest(image_name, token);
    free(token);
    if (!manifest) {
        fprintf(stderr, "Error retrieving image manifest.\n");
        cleanup_curl_global();
        return -1;
    }

    // Print manifest content
    printf("\nManifest Content:\n");
    printf("--------------------------------------------\n");
    printf("%s\n", manifest);
    printf("--------------------------------------------\n\n");

    // Parse manifest
    Manifest_parsed_info *manifest_info = parse_manifest(manifest);
    if (!manifest_info) {
        fprintf(stderr, "Error parsing image manifest.\n");
        free(manifest);
        cleanup_curl_global();
        return -1;
    }

    // Print layers information
    printf("Layers to download:\n");
    printf("--------------------------------------------\n");
    printLayerDigests(manifest_info->layersList);
    printf("--------------------------------------------\n\n");

    // Fetch and process each layer
    int i = 0;
    while (manifest_info->layersList != NULL) {
        printf("[*] Fetching layer %d\n", i);
        printf("--------------------------------------------------------\n");
        ++i;

        token = get_auth_token(image_name);
        if (!token) {
            fprintf(stderr, "Error retrieving authentication token for layer %d.\n", i);
            clean_resources(manifest, manifest_info);
            return -1;
        }

        char layer_url[1024];
        snprintf(layer_url, sizeof(layer_url), "https://registry.hub.docker.com/v2/%s/blobs/%s", image_name, manifest_info->layersList->digest);
        char *response = get_response(layer_url, token, "Layer");
        free(token);
        if (!response) {
            fprintf(stderr, "Error retrieving response for layer %d.\n", i);
            clean_resources(manifest, manifest_info);
            return -1;
        }
        free(response);

        manifest_info->layersList = manifest_info->layersList->next;
    }

    // Extract downloaded files
    printf("[+] All Files downloaded successfully.\n");
    for (int j = 0; j < i; j++) {
        char filename[256];
        snprintf(filename, sizeof(filename), "downloaded_file_%d.tar", j);
        printf("--------------------------------------------------------\n");
        printf("[*] Extracting %s.\n", filename);
        untar_and_remove(filename);
        printf("[+] %s Extracted successfully.\n", filename);
    }

    printf("\n\n[+] All Files extracted successfully");
    printf(" - Operation completed.\n\n");

    if (!chdir("../") == 0) {
        perror("Error changing directory");
    }

    clean_resources(manifest, manifest_info);
    return 0;
}

int untar_and_remove(const char * filename) {
  char command[512];

  // Construct the command to untar the file
  snprintf(command, sizeof(command), "tar -xf %s && rm -f %s", filename, filename);

  // Execute the command
  int result = system(command);

  if (result == -1) {
    perror("Failed to execute command");
    return -1;
  }

  return 0; // Return 0 on success
}

void clean_resources(char *manifest, Manifest_parsed_info *manifest_info) {
    if (manifest) free(manifest);
    if (manifest_info) free(manifest_info);
    cleanup_curl_global();
}
/**
 * VKCompute - Vulkan Compute Video Series
 * File utility functions implementation
 */

#define _GNU_SOURCE  // For strdup, readlink on Linux

#include "file_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #define PATH_SEPARATOR '\\'
#else
    #include <unistd.h>
    #include <limits.h>
    #include <libgen.h>
    #define PATH_SEPARATOR '/'
    #ifndef PATH_MAX
        #define PATH_MAX 4096
    #endif
#endif

// ============================================================================
// Read file
// ============================================================================

uint8_t* vkc_read_file(const char* filepath, size_t* out_size) {
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", filepath);
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        return NULL;
    }
    
    uint8_t* buffer = (uint8_t*)malloc((size_t)file_size);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    
    size_t read_size = fread(buffer, 1, (size_t)file_size, file);
    fclose(file);
    
    if (read_size != (size_t)file_size) {
        free(buffer);
        return NULL;
    }
    
    if (out_size) {
        *out_size = (size_t)file_size;
    }
    
    return buffer;
}

// ============================================================================
// Read SPIR-V file
// ============================================================================

uint32_t* vkc_read_spirv(const char* filepath, size_t* out_count) {
    size_t byte_size;
    uint8_t* bytes = vkc_read_file(filepath, &byte_size);
    
    if (!bytes) {
        return NULL;
    }
    
    // SPIR-V must be 4-byte aligned
    if (byte_size % 4 != 0) {
        fprintf(stderr, "Invalid SPIR-V file (not aligned): %s\n", filepath);
        free(bytes);
        return NULL;
    }
    
    // Check SPIR-V magic number
    uint32_t magic = *(uint32_t*)bytes;
    if (magic != 0x07230203) {
        fprintf(stderr, "Invalid SPIR-V magic number in: %s\n", filepath);
        free(bytes);
        return NULL;
    }
    
    if (out_count) {
        *out_count = byte_size / 4;
    }
    
    return (uint32_t*)bytes;
}

// ============================================================================
// File exists check
// ============================================================================

bool vkc_file_exists(const char* filepath) {
#ifdef _WIN32
    DWORD attrib = GetFileAttributesA(filepath);
    return (attrib != INVALID_FILE_ATTRIBUTES && 
            !(attrib & FILE_ATTRIBUTE_DIRECTORY));
#else
    return access(filepath, F_OK) == 0;
#endif
}

// ============================================================================
// Get executable directory
// ============================================================================

char* vkc_get_exe_dir(void) {
    char* path = NULL;
    
#ifdef _WIN32
    char buffer[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, buffer, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        // Find last backslash
        char* last_sep = strrchr(buffer, '\\');
        if (last_sep) {
            *last_sep = '\0';
            path = strdup(buffer);
        }
    }
#elif defined(__APPLE__)
    char buffer[PATH_MAX];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0) {
        char* dir = dirname(buffer);
        path = strdup(dir);
    }
#else
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        char* dir = dirname(buffer);
        path = strdup(dir);
    }
#endif
    
    return path;
}

// ============================================================================
// Path relative to executable
// ============================================================================

char* vkc_path_relative_to_exe(const char* relative_path) {
    char* exe_dir = vkc_get_exe_dir();
    if (!exe_dir) {
        return strdup(relative_path);
    }
    
    size_t exe_len = strlen(exe_dir);
    size_t rel_len = strlen(relative_path);
    size_t total_len = exe_len + 1 + rel_len + 1;
    
    char* full_path = (char*)malloc(total_len);
    if (!full_path) {
        free(exe_dir);
        return NULL;
    }
    
    snprintf(full_path, total_len, "%s%c%s", exe_dir, PATH_SEPARATOR, relative_path);
    free(exe_dir);
    
    return full_path;
}

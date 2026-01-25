/**
 * VKCompute - Vulkan Compute Video Series
 * File utility functions
 */

#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Read entire file into memory
// Returns allocated buffer (caller must free) or NULL on error
// Sets out_size to the number of bytes read
uint8_t* vkc_read_file(const char* filepath, size_t* out_size);

// Read file as uint32_t array (for SPIR-V)
// Returns allocated buffer (caller must free) or NULL on error
// Sets out_count to the number of uint32_t elements
uint32_t* vkc_read_spirv(const char* filepath, size_t* out_count);

// Check if file exists
bool vkc_file_exists(const char* filepath);

// Get executable directory path
// Returns allocated string (caller must free) or NULL on error
char* vkc_get_exe_dir(void);

// Build path relative to executable
// Returns allocated string (caller must free) or NULL on error
char* vkc_path_relative_to_exe(const char* relative_path);

#ifdef __cplusplus
}
#endif

#endif // FILE_UTILS_H

/**
 * VKCompute - Vulkan Compute Video Series
 * Utility macros and helpers
 */

#ifndef VK_UTILS_H
#define VK_UTILS_H

#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Error handling macros
// ============================================================================

const char* vkc_result_string(VkResult result);

#define VK_CHECK(call) \
    do { \
        VkResult result = (call); \
        if (result != VK_SUCCESS) { \
            fprintf(stderr, "Vulkan error at %s:%d: %s returned %s\n", \
                    __FILE__, __LINE__, #call, vkc_result_string(result)); \
            exit(EXIT_FAILURE); \
        } \
    } while(0)

#define VK_CHECK_RETURN(call) \
    do { \
        VkResult result = (call); \
        if (result != VK_SUCCESS) { \
            fprintf(stderr, "Vulkan error at %s:%d: %s returned %s\n", \
                    __FILE__, __LINE__, #call, vkc_result_string(result)); \
            return result; \
        } \
    } while(0)

// ============================================================================
// Logging macros
// ============================================================================

#define VKC_LOG_INFO(...) \
    do { fprintf(stdout, "[INFO] "); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); } while(0)

#define VKC_LOG_WARN(...) \
    do { fprintf(stderr, "[WARN] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)

#define VKC_LOG_ERROR(...) \
    do { fprintf(stderr, "[ERROR] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)

#ifdef NDEBUG
    #define VKC_LOG_DEBUG(...) ((void)0)
#else
    #define VKC_LOG_DEBUG(...) \
        do { fprintf(stdout, "[DEBUG] "); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); } while(0)
#endif

// ============================================================================
// Utility functions
// ============================================================================

uint32_t vkc_find_memory_type(VkPhysicalDevice physical_device,
                              uint32_t type_filter,
                              VkMemoryPropertyFlags properties);

void vkc_print_device_info(VkPhysicalDevice device);

int32_t vkc_find_compute_queue_family(VkPhysicalDevice device);

// ============================================================================
// Math helpers for compute
// ============================================================================

// Calculate workgroup count for 1D dispatch
static inline uint32_t vkc_div_ceil(uint32_t x, uint32_t y) {
    return (x + y - 1) / y;
}

// Align value up to alignment
static inline uint32_t vkc_align_up(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

// ============================================================================
// Cross-platform timer
// ============================================================================

double vkc_get_time_ms(void);

#ifdef __cplusplus
}
#endif

#endif // VK_UTILS_H

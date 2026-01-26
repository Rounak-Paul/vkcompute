/**
 * VKCompute 
 * Utility functions implementation
 */

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #define _POSIX_C_SOURCE 199309L
    #include <time.h>
#endif

#include "vk_utils.h"
#include <string.h>

// ============================================================================
// VkResult to string conversion
// ============================================================================

const char* vkc_result_string(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
        case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
        case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
        case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        default: return "UNKNOWN_VK_RESULT";
    }
}

// ============================================================================
// Memory type finding
// ============================================================================

uint32_t vkc_find_memory_type(VkPhysicalDevice physical_device,
                            uint32_t type_filter,
                            VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);
    
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    VKC_LOG_ERROR("Failed to find suitable memory type!");
    return 0;
}

// ============================================================================
// Device info
// ============================================================================

void vkc_print_device_info(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);
    
    const char* device_type = "Unknown";
    switch (props.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: device_type = "Integrated GPU"; break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: device_type = "Discrete GPU"; break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: device_type = "Virtual GPU"; break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU: device_type = "CPU"; break;
        default: break;
    }
    
    VKC_LOG_INFO("=== GPU Info ===");
    VKC_LOG_INFO("  Name: %s", props.deviceName);
    VKC_LOG_INFO("  Type: %s", device_type);
    VKC_LOG_INFO("  API Version: %d.%d.%d",
                VK_API_VERSION_MAJOR(props.apiVersion),
                VK_API_VERSION_MINOR(props.apiVersion),
                VK_API_VERSION_PATCH(props.apiVersion));
    VKC_LOG_INFO("  Driver Version: %d.%d.%d",
                VK_API_VERSION_MAJOR(props.driverVersion),
                VK_API_VERSION_MINOR(props.driverVersion),
                VK_API_VERSION_PATCH(props.driverVersion));
    
    // Compute limits
    VKC_LOG_INFO("=== Compute Limits ===");
    VKC_LOG_INFO("  Max Compute Workgroup Count: [%u, %u, %u]",
                props.limits.maxComputeWorkGroupCount[0],
                props.limits.maxComputeWorkGroupCount[1],
                props.limits.maxComputeWorkGroupCount[2]);
    VKC_LOG_INFO("  Max Compute Workgroup Size: [%u, %u, %u]",
                props.limits.maxComputeWorkGroupSize[0],
                props.limits.maxComputeWorkGroupSize[1],
                props.limits.maxComputeWorkGroupSize[2]);
    VKC_LOG_INFO("  Max Compute Workgroup Invocations: %u",
                props.limits.maxComputeWorkGroupInvocations);
    VKC_LOG_INFO("  Max Compute Shared Memory: %u bytes",
                props.limits.maxComputeSharedMemorySize);
}

// ============================================================================
// Queue family finding
// ============================================================================

int32_t vkc_find_compute_queue_family(VkPhysicalDevice device) {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);
    
    VkQueueFamilyProperties* queue_families = 
        malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);
    
    int32_t compute_family = -1;
    int32_t dedicated_compute = -1;
    
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            compute_family = (int32_t)i;
            
            // Prefer dedicated compute queue (no graphics)
            if (!(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                dedicated_compute = (int32_t)i;
            }
        }
    }
    
    free(queue_families);
    
    // Return dedicated compute queue if available, otherwise any compute queue
    return (dedicated_compute >= 0) ? dedicated_compute : compute_family;
}

// ============================================================================
// Cross-platform high-resolution timer
// ============================================================================

double vkc_get_time_ms(void) {
#ifdef _WIN32
    static LARGE_INTEGER frequency = {0};
    static int initialized = 0;
    
    if (!initialized) {
        QueryPerformanceFrequency(&frequency);
        initialized = 1;
    }
    
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart * 1000.0 / (double)frequency.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
#endif
}

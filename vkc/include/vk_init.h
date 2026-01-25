/**
 * VKCompute - Vulkan Compute Video Series
 * Common Vulkan initialization utilities
 */

#ifndef VK_INIT_H
#define VK_INIT_H

#include <vulkan/vulkan.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Configuration
// ============================================================================

typedef struct VkcConfig {
    const char* app_name;
    uint32_t app_version;
    bool enable_validation;
    bool prefer_discrete_gpu;
    uint32_t required_queue_flags;  // e.g., VK_QUEUE_COMPUTE_BIT
} VkcConfig;

// Default configuration
VkcConfig vkc_config_default(void);

// ============================================================================
// Context - Main Vulkan state
// ============================================================================

typedef struct VkcContext {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue compute_queue;
    uint32_t compute_queue_family;
    
    VkPhysicalDeviceProperties device_props;
    VkPhysicalDeviceMemoryProperties memory_props;
    
    VkDebugUtilsMessengerEXT debug_messenger;
    bool validation_enabled;
} VkcContext;

// Initialize Vulkan context
VkResult vkc_init(VkcContext* ctx, const VkcConfig* config);

// Cleanup Vulkan context
void vkc_cleanup(VkcContext* ctx);

// ============================================================================
// Helper functions
// ============================================================================

// Create a compute command pool
VkResult vkc_create_command_pool(VkcContext* ctx, VkCommandPool* pool);

// Create a command buffer
VkResult vkc_create_command_buffer(VkcContext* ctx, VkCommandPool pool, 
                                   VkCommandBuffer* cmd);

// Submit command buffer and wait
VkResult vkc_submit_and_wait(VkcContext* ctx, VkCommandBuffer cmd);

// Create a fence
VkResult vkc_create_fence(VkcContext* ctx, VkFence* fence, bool signaled);

// ============================================================================
// Buffer helpers
// ============================================================================

typedef struct VkcBuffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize size;
    void* mapped;  // Non-null if persistently mapped
} VkcBuffer;

// Create a buffer with memory
VkResult vkc_create_buffer(VkcContext* ctx, VkDeviceSize size,
                           VkBufferUsageFlags usage,
                           VkMemoryPropertyFlags mem_props,
                           VkcBuffer* buffer);

// Destroy a buffer
void vkc_destroy_buffer(VkcContext* ctx, VkcBuffer* buffer);

// Map buffer memory
VkResult vkc_map_buffer(VkcContext* ctx, VkcBuffer* buffer, void** data);

// Unmap buffer memory
void vkc_unmap_buffer(VkcContext* ctx, VkcBuffer* buffer);

// Copy data to buffer
VkResult vkc_upload_buffer(VkcContext* ctx, VkcBuffer* buffer, 
                           const void* data, VkDeviceSize size);

// Copy data from buffer
VkResult vkc_download_buffer(VkcContext* ctx, VkcBuffer* buffer,
                             void* data, VkDeviceSize size);

// ============================================================================
// Shader helpers
// ============================================================================

// Load SPIR-V shader from file
VkResult vkc_load_shader(VkcContext* ctx, const char* filepath,
                         VkShaderModule* shader);

// Create compute pipeline
VkResult vkc_create_compute_pipeline(VkcContext* ctx,
                                     VkShaderModule shader,
                                     VkPipelineLayout layout,
                                     VkPipeline* pipeline);

// ============================================================================
// Simple compute runner (for Episode 01)
// ============================================================================

// Run a simple compute shader with input/output buffers
// This is a convenience function that handles all the boilerplate:
// - Loads shader, creates descriptors, pipeline, command buffer
// - Dispatches compute work and waits for completion
// - Cleans up all temporary resources
VkResult vkc_run_simple_compute(VkcContext* ctx,
                                const char* shader_path,
                                VkcBuffer* input,
                                VkcBuffer* output,
                                uint32_t element_count);

// Print device info (name, type, etc)
void vkc_print_device_info(VkPhysicalDevice device);

#ifdef __cplusplus
}
#endif

#endif // VK_INIT_H

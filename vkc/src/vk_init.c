/**
 * VKCompute - Vulkan Compute Video Series
 * Vulkan initialization implementation
 */

#include "vk_init.h"
#include "vk_utils.h"
#include "file_utils.h"
#include <string.h>
#include <stdlib.h>

// ============================================================================
// Validation layer callback
// ============================================================================

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) 
{
    (void)type;
    (void)user_data;
    
    const char* severity_str = "UNKNOWN";
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        severity_str = "ERROR";
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        severity_str = "WARNING";
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        severity_str = "INFO";
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        severity_str = "VERBOSE";
    }
    
    fprintf(stderr, "[Vulkan %s] %s\n", severity_str, callback_data->pMessage);
    
    return VK_FALSE;
}

// ============================================================================
// Configuration
// ============================================================================

VkcConfig vkc_config_default(void) {
    VkcConfig config = {
        .app_name = "VKCompute",
        .app_version = VK_MAKE_VERSION(1, 0, 0),
        .enable_validation = true,
        .prefer_discrete_gpu = true,
        .required_queue_flags = VK_QUEUE_COMPUTE_BIT
    };
    return config;
}

// ============================================================================
// Instance creation
// ============================================================================

static VkResult create_instance(VkcContext* ctx, const VkcConfig* config) {
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = config->app_name,
        .applicationVersion = config->app_version,
        .pEngineName = "VKCompute",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_2
    };
    
    // Extensions
    const char* extensions[] = {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };
    uint32_t extension_count = config->enable_validation ? 1 : 0;
    
    // Validation layers
    const char* layers[] = {
        "VK_LAYER_KHRONOS_validation"
    };
    uint32_t layer_count = 0;
    
    if (config->enable_validation) {
        // Check if validation layer is available
        uint32_t available_count;
        vkEnumerateInstanceLayerProperties(&available_count, NULL);
        
        VkLayerProperties* available = malloc(sizeof(VkLayerProperties) * available_count);
        vkEnumerateInstanceLayerProperties(&available_count, available);
        
        bool found = false;
        for (uint32_t i = 0; i < available_count; i++) {
            if (strcmp(available[i].layerName, layers[0]) == 0) {
                found = true;
                break;
            }
        }
        free(available);
        
        if (found) {
            layer_count = 1;
            ctx->validation_enabled = true;
            VKC_LOG_INFO("Validation layers enabled");
        } else {
            VKC_LOG_WARN("Validation layer not found, continuing without validation");
            extension_count = 0;
        }
    }
    
    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = extensions,
        .enabledLayerCount = layer_count,
        .ppEnabledLayerNames = layers
    };
    
    VK_CHECK_RETURN(vkCreateInstance(&create_info, NULL, &ctx->instance));
    
    // Create debug messenger
    if (ctx->validation_enabled) {
        VkDebugUtilsMessengerCreateInfoEXT debug_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debug_callback
        };
        
        PFN_vkCreateDebugUtilsMessengerEXT func = 
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                ctx->instance, "vkCreateDebugUtilsMessengerEXT");
        
        if (func) {
            func(ctx->instance, &debug_info, NULL, &ctx->debug_messenger);
        }
    }
    
    return VK_SUCCESS;
}

// ============================================================================
// Physical device selection
// ============================================================================

static VkResult select_physical_device(VkcContext* ctx, const VkcConfig* config) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(ctx->instance, &device_count, NULL);
    
    if (device_count == 0) {
        VKC_LOG_ERROR("No Vulkan-capable GPU found!");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    
    VkPhysicalDevice* devices = malloc(sizeof(VkPhysicalDevice) * device_count);
    vkEnumeratePhysicalDevices(ctx->instance, &device_count, devices);
    
    // Score devices
    int best_score = -1;
    VkPhysicalDevice best_device = VK_NULL_HANDLE;
    
    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[i], &props);
        
        // Check for compute queue
        int32_t compute_family = vkc_find_compute_queue_family(devices[i]);
        if (compute_family < 0) {
            continue;
        }
        
        int score = 0;
        
        // Prefer discrete GPUs
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        } else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            score += 100;
        }
        
        // Higher API version is better
        score += VK_API_VERSION_MINOR(props.apiVersion) * 10;
        
        if (score > best_score || !config->prefer_discrete_gpu) {
            best_score = score;
            best_device = devices[i];
        }
    }
    
    free(devices);
    
    if (best_device == VK_NULL_HANDLE) {
        VKC_LOG_ERROR("No suitable GPU found!");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    
    ctx->physical_device = best_device;
    vkGetPhysicalDeviceProperties(ctx->physical_device, &ctx->device_props);
    vkGetPhysicalDeviceMemoryProperties(ctx->physical_device, &ctx->memory_props);
    
    VKC_LOG_INFO("Selected GPU: %s", ctx->device_props.deviceName);
    
    return VK_SUCCESS;
}

// ============================================================================
// Logical device creation
// ============================================================================

static VkResult create_device(VkcContext* ctx) {
    ctx->compute_queue_family = (uint32_t)vkc_find_compute_queue_family(ctx->physical_device);
    
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = ctx->compute_queue_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };
    
    VkPhysicalDeviceFeatures features = {0};
    
    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .pEnabledFeatures = &features
    };
    
    VK_CHECK_RETURN(vkCreateDevice(ctx->physical_device, &device_info, NULL, &ctx->device));
    
    vkGetDeviceQueue(ctx->device, ctx->compute_queue_family, 0, &ctx->compute_queue);
    
    return VK_SUCCESS;
}

// ============================================================================
// Public API
// ============================================================================

VkResult vkc_init(VkcContext* ctx, const VkcConfig* config) {
    memset(ctx, 0, sizeof(VkcContext));
    
    VkcConfig default_config = vkc_config_default();
    if (!config) {
        config = &default_config;
    }
    
    VK_CHECK_RETURN(create_instance(ctx, config));
    VK_CHECK_RETURN(select_physical_device(ctx, config));
    VK_CHECK_RETURN(create_device(ctx));
    
    VKC_LOG_INFO("Vulkan initialized successfully");
    
    return VK_SUCCESS;
}

void vkc_cleanup(VkcContext* ctx) {
    if (ctx->device) {
        vkDeviceWaitIdle(ctx->device);
        vkDestroyDevice(ctx->device, NULL);
    }
    
    if (ctx->debug_messenger && ctx->instance) {
        PFN_vkDestroyDebugUtilsMessengerEXT func = 
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                ctx->instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) {
            func(ctx->instance, ctx->debug_messenger, NULL);
        }
    }
    
    if (ctx->instance) {
        vkDestroyInstance(ctx->instance, NULL);
    }
    
    memset(ctx, 0, sizeof(VkcContext));
    VKC_LOG_INFO("Vulkan cleaned up");
}

VkResult vkc_create_command_pool(VkcContext* ctx, VkCommandPool* pool) {
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = ctx->compute_queue_family,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    };
    
    return vkCreateCommandPool(ctx->device, &pool_info, NULL, pool);
}

VkResult vkc_create_command_buffer(VkcContext* ctx, VkCommandPool pool, 
                                   VkCommandBuffer* cmd) {
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    
    return vkAllocateCommandBuffers(ctx->device, &alloc_info, cmd);
}

VkResult vkc_submit_and_wait(VkcContext* ctx, VkCommandBuffer cmd) {
    VkFence fence;
    VK_CHECK_RETURN(vkc_create_fence(ctx, &fence, false));
    
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    
    VK_CHECK_RETURN(vkQueueSubmit(ctx->compute_queue, 1, &submit_info, fence));
    VK_CHECK_RETURN(vkWaitForFences(ctx->device, 1, &fence, VK_TRUE, UINT64_MAX));
    
    vkDestroyFence(ctx->device, fence, NULL);
    
    return VK_SUCCESS;
}

VkResult vkc_create_fence(VkcContext* ctx, VkFence* fence, bool signaled) {
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0
    };
    
    return vkCreateFence(ctx->device, &fence_info, NULL, fence);
}

// ============================================================================
// Buffer helpers
// ============================================================================

VkResult vkc_create_buffer(VkcContext* ctx, VkDeviceSize size,
                           VkBufferUsageFlags usage,
                           VkMemoryPropertyFlags mem_props,
                           VkcBuffer* buffer) {
    memset(buffer, 0, sizeof(VkcBuffer));
    buffer->size = size;
    
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    
    VK_CHECK_RETURN(vkCreateBuffer(ctx->device, &buffer_info, NULL, &buffer->buffer));
    
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(ctx->device, buffer->buffer, &mem_req);
    
    uint32_t mem_type = vkc_find_memory_type(ctx->physical_device, 
                                              mem_req.memoryTypeBits, 
                                              mem_props);
    
    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = mem_type
    };
    
    VK_CHECK_RETURN(vkAllocateMemory(ctx->device, &alloc_info, NULL, &buffer->memory));
    VK_CHECK_RETURN(vkBindBufferMemory(ctx->device, buffer->buffer, buffer->memory, 0));
    
    return VK_SUCCESS;
}

void vkc_destroy_buffer(VkcContext* ctx, VkcBuffer* buffer) {
    if (buffer->mapped) {
        vkc_unmap_buffer(ctx, buffer);
    }
    if (buffer->buffer) {
        vkDestroyBuffer(ctx->device, buffer->buffer, NULL);
    }
    if (buffer->memory) {
        vkFreeMemory(ctx->device, buffer->memory, NULL);
    }
    memset(buffer, 0, sizeof(VkcBuffer));
}

VkResult vkc_map_buffer(VkcContext* ctx, VkcBuffer* buffer, void** data) {
    VkResult result = vkMapMemory(ctx->device, buffer->memory, 0, buffer->size, 0, data);
    if (result == VK_SUCCESS) {
        buffer->mapped = *data;
    }
    return result;
}

void vkc_unmap_buffer(VkcContext* ctx, VkcBuffer* buffer) {
    if (buffer->mapped) {
        vkUnmapMemory(ctx->device, buffer->memory);
        buffer->mapped = NULL;
    }
}

VkResult vkc_upload_buffer(VkcContext* ctx, VkcBuffer* buffer, 
                           const void* data, VkDeviceSize size) {
    void* mapped;
    VK_CHECK_RETURN(vkc_map_buffer(ctx, buffer, &mapped));
    memcpy(mapped, data, (size_t)size);
    vkc_unmap_buffer(ctx, buffer);
    return VK_SUCCESS;
}

VkResult vkc_download_buffer(VkcContext* ctx, VkcBuffer* buffer,
                             void* data, VkDeviceSize size) {
    void* mapped;
    VK_CHECK_RETURN(vkc_map_buffer(ctx, buffer, &mapped));
    memcpy(data, mapped, (size_t)size);
    vkc_unmap_buffer(ctx, buffer);
    return VK_SUCCESS;
}

// ============================================================================
// Shader helpers
// ============================================================================

VkResult vkc_load_shader(VkcContext* ctx, const char* filepath,
                         VkShaderModule* shader) {
    size_t code_size;
    uint32_t* code = vkc_read_spirv(filepath, &code_size);
    
    if (!code) {
        VKC_LOG_ERROR("Failed to load shader: %s", filepath);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    
    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code_size * sizeof(uint32_t),
        .pCode = code
    };
    
    VkResult result = vkCreateShaderModule(ctx->device, &create_info, NULL, shader);
    free(code);
    
    return result;
}

VkResult vkc_create_compute_pipeline(VkcContext* ctx,
                                     VkShaderModule shader,
                                     VkPipelineLayout layout,
                                     VkPipeline* pipeline) {
    VkPipelineShaderStageCreateInfo stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shader,
        .pName = "main"
    };
    
    VkComputePipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stage_info,
        .layout = layout
    };
    
    return vkCreateComputePipelines(ctx->device, VK_NULL_HANDLE, 1, 
                                    &pipeline_info, NULL, pipeline);
}

// ============================================================================
// Simple compute runner (hides complexity for Episode 01)
// ============================================================================

VkResult vkc_run_simple_compute(VkcContext* ctx,
                                const char* shader_path,
                                VkcBuffer* input,
                                VkcBuffer* output,
                                uint32_t element_count) {
    VkResult result = VK_SUCCESS;
    
    // Load shader
    VkShaderModule shader;
    result = vkc_load_shader(ctx, shader_path, &shader);
    if (result != VK_SUCCESS) return result;
    
    // Create descriptor set layout (2 storage buffers: input + output)
    VkDescriptorSetLayoutBinding bindings[2] = {
        { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
        { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT }
    };
    
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2, .pBindings = bindings
    };
    
    VkDescriptorSetLayout desc_layout;
    result = vkCreateDescriptorSetLayout(ctx->device, &layout_info, NULL, &desc_layout);
    if (result != VK_SUCCESS) { vkDestroyShaderModule(ctx->device, shader, NULL); return result; }
    
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipe_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1, .pSetLayouts = &desc_layout
    };
    
    VkPipelineLayout pipe_layout;
    result = vkCreatePipelineLayout(ctx->device, &pipe_layout_info, NULL, &pipe_layout);
    if (result != VK_SUCCESS) goto cleanup_desc_layout;
    
    // Create compute pipeline
    VkPipeline pipeline;
    result = vkc_create_compute_pipeline(ctx, shader, pipe_layout, &pipeline);
    if (result != VK_SUCCESS) goto cleanup_pipe_layout;
    
    // Create descriptor pool
    VkDescriptorPoolSize pool_size = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &pool_size
    };
    
    VkDescriptorPool desc_pool;
    result = vkCreateDescriptorPool(ctx->device, &pool_info, NULL, &desc_pool);
    if (result != VK_SUCCESS) goto cleanup_pipeline;
    
    // Allocate descriptor set
    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = desc_pool, .descriptorSetCount = 1, .pSetLayouts = &desc_layout
    };
    
    VkDescriptorSet desc_set;
    result = vkAllocateDescriptorSets(ctx->device, &alloc_info, &desc_set);
    if (result != VK_SUCCESS) goto cleanup_pool;
    
    // Update descriptor set with buffer info
    VkDescriptorBufferInfo buf_infos[2] = {
        { input->buffer, 0, VK_WHOLE_SIZE },
        { output->buffer, 0, VK_WHOLE_SIZE }
    };
    
    VkWriteDescriptorSet writes[2] = {
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = desc_set,
          .dstBinding = 0, .descriptorCount = 1, 
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buf_infos[0] },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = desc_set,
          .dstBinding = 1, .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buf_infos[1] }
    };
    vkUpdateDescriptorSets(ctx->device, 2, writes, 0, NULL);
    
    // Create command pool and buffer
    VkCommandPool cmd_pool;
    result = vkc_create_command_pool(ctx, &cmd_pool);
    if (result != VK_SUCCESS) goto cleanup_pool;
    
    VkCommandBuffer cmd;
    result = vkc_create_command_buffer(ctx, cmd_pool, &cmd);
    if (result != VK_SUCCESS) goto cleanup_cmd_pool;
    
    // Record command buffer
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    
    result = vkBeginCommandBuffer(cmd, &begin_info);
    if (result != VK_SUCCESS) goto cleanup_cmd_pool;
    
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe_layout,
                            0, 1, &desc_set, 0, NULL);
    
    // Dispatch with workgroup size of 256 (standard choice)
    uint32_t workgroup_count = vkc_div_ceil(element_count, 256);
    vkCmdDispatch(cmd, workgroup_count, 1, 1);
    
    result = vkEndCommandBuffer(cmd);
    if (result != VK_SUCCESS) goto cleanup_cmd_pool;
    
    // Submit and wait
    result = vkc_submit_and_wait(ctx, cmd);
    
    // Cleanup (in reverse order)
cleanup_cmd_pool:
    vkDestroyCommandPool(ctx->device, cmd_pool, NULL);
cleanup_pool:
    vkDestroyDescriptorPool(ctx->device, desc_pool, NULL);
cleanup_pipeline:
    vkDestroyPipeline(ctx->device, pipeline, NULL);
cleanup_pipe_layout:
    vkDestroyPipelineLayout(ctx->device, pipe_layout, NULL);
cleanup_desc_layout:
    vkDestroyDescriptorSetLayout(ctx->device, desc_layout, NULL);
    vkDestroyShaderModule(ctx->device, shader, NULL);
    
    return result;
}

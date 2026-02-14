/**
 * VKCompute - Chapter 02: Buffer Types and Staging
 * 
 * Learn about different buffer memory types and how to use staging buffers
 * for efficient GPU data transfers.
 * 
 * Topics covered:
 * - Host visible vs device local memory
 * - Staging buffers for high-performance transfers
 * - Buffer copy commands
 * - Memory heaps and types exploration
 * - Coherent vs non-coherent memory
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "vk_init.h"
#include "vk_utils.h"
#include "file_utils.h"

#define ARRAY_SIZE (1024 * 1024)  // 1 million elements

// Timer helper

// Print memory properties
static void print_memory_info(VkPhysicalDevice device) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(device, &mem_props);
    
    printf("\n=== Memory Heaps ===\n");
    for (uint32_t i = 0; i < mem_props.memoryHeapCount; i++) {
        const char* type = (mem_props.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) 
                          ? "Device Local" : "System";
        printf("  Heap %u: %.2f GB (%s)\n", i, 
               mem_props.memoryHeaps[i].size / (1024.0 * 1024.0 * 1024.0), type);
    }
    
    printf("\n=== Memory Types ===\n");
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        VkMemoryPropertyFlags flags = mem_props.memoryTypes[i].propertyFlags;
        printf("  Type %u (Heap %u): ", i, mem_props.memoryTypes[i].heapIndex);
        
        if (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) printf("DEVICE_LOCAL ");
        if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) printf("HOST_VISIBLE ");
        if (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) printf("HOST_COHERENT ");
        if (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) printf("HOST_CACHED ");
        if (flags == 0) printf("(none)");
        printf("\n");
    }
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    
    printf("==============================================\n");
    printf("  VKCompute - Chapter 02: Buffer Types\n");
    printf("==============================================\n\n");
    
    // Initialize Vulkan
    VkcContext ctx;
    VkcConfig config = vkc_config_default();
    config.app_name = "CH02 Buffers";
    VK_CHECK(vkc_init(&ctx, &config));
    
    // Print memory information
    print_memory_info(ctx.physical_device);
    
    // Create command pool
    VkCommandPool command_pool;
    VK_CHECK(vkc_create_command_pool(&ctx, &command_pool));
    
    VkDeviceSize buffer_size = ARRAY_SIZE * sizeof(float);
    printf("\nBuffer size: %.2f MB\n", buffer_size / (1024.0 * 1024.0));
    
    // ========================================================================
    // Method 1: Host Visible Buffer (Direct access, slower compute)
    // ========================================================================
    
    printf("\n--- Method 1: Host Visible Buffer ---\n");
    
    VkcBuffer host_buffer;
    VK_CHECK(vkc_create_buffer(&ctx, buffer_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &host_buffer));
    
    // Fill with data
    float* data = malloc(buffer_size);
    for (int i = 0; i < ARRAY_SIZE; i++) {
        data[i] = (float)i;
    }
    
    double start = vkc_get_time_ms();
    VK_CHECK(vkc_upload_buffer(&ctx, &host_buffer, data, buffer_size));
    double upload_time = vkc_get_time_ms() - start;
    printf("  Upload time: %.2f ms (%.2f GB/s)\n", upload_time,
           (buffer_size / (1024.0 * 1024.0 * 1024.0)) / (upload_time / 1000.0));
    
    // ========================================================================
    // Method 2: Staging Buffer -> Device Local (Optimal for compute)
    // ========================================================================
    
    printf("\n--- Method 2: Staging Buffer + Device Local ---\n");
    
    // Staging buffer (host visible, used for transfer)
    VkcBuffer staging_buffer;
    VK_CHECK(vkc_create_buffer(&ctx, buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging_buffer));
    
    // Device local buffer (fast GPU access)
    VkcBuffer device_buffer;
    VkResult device_result = vkc_create_buffer(&ctx, buffer_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &device_buffer);
    
    if (device_result != VK_SUCCESS) {
        // Fallback: some GPUs don't have separate device local memory
        printf("  Note: No dedicated device local memory, using host visible\n");
        VK_CHECK(vkc_create_buffer(&ctx, buffer_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &device_buffer));
    }
    
    // Upload to staging
    start = vkc_get_time_ms();
    VK_CHECK(vkc_upload_buffer(&ctx, &staging_buffer, data, buffer_size));
    double staging_upload = vkc_get_time_ms() - start;
    printf("  Staging upload: %.2f ms\n", staging_upload);
    
    // Copy from staging to device (GPU-side copy)
    VkCommandBuffer cmd;
    VK_CHECK(vkc_create_command_buffer(&ctx, command_pool, &cmd));
    
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
    
    VkBufferCopy copy_region = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = buffer_size
    };
    vkCmdCopyBuffer(cmd, staging_buffer.buffer, device_buffer.buffer, 1, &copy_region);
    
    VK_CHECK(vkEndCommandBuffer(cmd));
    
    start = vkc_get_time_ms();
    VK_CHECK(vkc_submit_and_wait(&ctx, cmd));
    double copy_time = vkc_get_time_ms() - start;
    printf("  GPU copy time: %.2f ms (%.2f GB/s)\n", copy_time,
           (buffer_size / (1024.0 * 1024.0 * 1024.0)) / (copy_time / 1000.0));
    
    // ========================================================================
    // Run compute shader on device local buffer
    // ========================================================================
    
    printf("\n--- Running Compute Shader ---\n");
    
    char* shader_path = vkc_path_relative_to_exe("shaders/ch02_buffers/square.comp.spv");
    VkShaderModule shader;
    VK_CHECK(vkc_load_shader(&ctx, shader_path, &shader));
    free(shader_path);
    
    // Create descriptor set layout
    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
    };
    
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding
    };
    
    VkDescriptorSetLayout desc_layout;
    VK_CHECK(vkCreateDescriptorSetLayout(ctx.device, &layout_info, NULL, &desc_layout));
    
    // Pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &desc_layout
    };
    
    VkPipelineLayout pipeline_layout;
    VK_CHECK(vkCreatePipelineLayout(ctx.device, &pipeline_layout_info, NULL, &pipeline_layout));
    
    // Create pipeline
    VkPipeline pipeline;
    VK_CHECK(vkc_create_compute_pipeline(&ctx, shader, pipeline_layout, &pipeline));
    
    // Descriptor pool and set
    VkDescriptorPoolSize pool_size = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size
    };
    
    VkDescriptorPool desc_pool;
    VK_CHECK(vkCreateDescriptorPool(ctx.device, &pool_info, NULL, &desc_pool));
    
    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = desc_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &desc_layout
    };
    
    VkDescriptorSet desc_set;
    VK_CHECK(vkAllocateDescriptorSets(ctx.device, &alloc_info, &desc_set));
    
    VkDescriptorBufferInfo buffer_info = {
        .buffer = device_buffer.buffer,
        .offset = 0,
        .range = VK_WHOLE_SIZE
    };
    
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = desc_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &buffer_info
    };
    
    vkUpdateDescriptorSets(ctx.device, 1, &write, 0, NULL);
    
    // Record and run compute
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
    
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                            0, 1, &desc_set, 0, NULL);
    vkCmdDispatch(cmd, vkc_div_ceil(ARRAY_SIZE, 256), 1, 1);
    
    VK_CHECK(vkEndCommandBuffer(cmd));
    
    start = vkc_get_time_ms();
    VK_CHECK(vkc_submit_and_wait(&ctx, cmd));
    double compute_time = vkc_get_time_ms() - start;
    printf("  Compute time: %.2f ms\n", compute_time);
    
    // ========================================================================
    // Read back results via staging
    // ========================================================================
    
    printf("\n--- Reading Back Results ---\n");
    
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
    vkCmdCopyBuffer(cmd, device_buffer.buffer, staging_buffer.buffer, 1, &copy_region);
    VK_CHECK(vkEndCommandBuffer(cmd));
    
    start = vkc_get_time_ms();
    VK_CHECK(vkc_submit_and_wait(&ctx, cmd));
    double readback_copy = vkc_get_time_ms() - start;
    printf("  GPU readback copy: %.2f ms\n", readback_copy);
    
    float* output_result = malloc(buffer_size);
    start = vkc_get_time_ms();
    vkc_download_buffer(&ctx, &staging_buffer, output_result, buffer_size);
    double download_time = vkc_get_time_ms() - start;
    printf("  Download time: %.2f ms\n", download_time);
    
    // Verify results (square of input)
    printf("\n=== Verification ===\n");
    printf("First 5 values:\n");
    for (int i = 0; i < 5; i++) {
        printf("  %d² = %.0f (expected %.0f)\n", i, output_result[i], (float)(i * i));
    }
    
    bool success = true;
    for (int i = 0; i < ARRAY_SIZE; i++) {
        float expected = (float)i * (float)i;
        if (output_result[i] != expected) {
            printf("ERROR at %d: got %.0f, expected %.0f\n", i, output_result[i], expected);
            success = false;
            break;
        }
    }
    
    if (success) {
        printf("\n[ok] All %d values computed correctly!\n", ARRAY_SIZE);
    }
    
    // Cleanup
    free(data);
    free(output_result);
    vkDestroyPipeline(ctx.device, pipeline, NULL);
    vkDestroyPipelineLayout(ctx.device, pipeline_layout, NULL);
    vkDestroyDescriptorPool(ctx.device, desc_pool, NULL);
    vkDestroyDescriptorSetLayout(ctx.device, desc_layout, NULL);
    vkDestroyShaderModule(ctx.device, shader, NULL);
    vkDestroyCommandPool(ctx.device, command_pool, NULL);
    vkc_destroy_buffer(&ctx, &host_buffer);
    vkc_destroy_buffer(&ctx, &staging_buffer);
    vkc_destroy_buffer(&ctx, &device_buffer);
    vkc_cleanup(&ctx);
    
    printf("\nChapter 02 completed!\n");
    return 0;
}

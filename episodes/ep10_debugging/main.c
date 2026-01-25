/**
 * VKCompute - Episode 10: Debugging and Profiling
 * 
 * Learn to debug and profile Vulkan compute applications.
 * 
 * Topics covered:
 * - Validation layers in depth
 * - Debug object naming
 * - Debug markers and labels
 * - GPU timestamps for profiling
 * - Query pools for performance data
 * - Common errors and how to fix them
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "vk_init.h"
#include "vk_utils.h"
#include "file_utils.h"

#define ARRAY_SIZE (1024 * 1024)
#define NUM_ITERATIONS 100

// Debug utilities function pointers
PFN_vkSetDebugUtilsObjectNameEXT pfn_vkSetDebugUtilsObjectNameEXT = NULL;
PFN_vkCmdBeginDebugUtilsLabelEXT pfn_vkCmdBeginDebugUtilsLabelEXT = NULL;
PFN_vkCmdEndDebugUtilsLabelEXT pfn_vkCmdEndDebugUtilsLabelEXT = NULL;
PFN_vkCmdInsertDebugUtilsLabelEXT pfn_vkCmdInsertDebugUtilsLabelEXT = NULL;

static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

// Set debug name for a Vulkan object
static void set_object_name(VkDevice device, uint64_t object, 
                            VkObjectType type, const char* name) {
    if (!pfn_vkSetDebugUtilsObjectNameEXT) return;
    
    VkDebugUtilsObjectNameInfoEXT name_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = type,
        .objectHandle = object,
        .pObjectName = name
    };
    pfn_vkSetDebugUtilsObjectNameEXT(device, &name_info);
}

// Begin a debug label region
static void cmd_begin_label(VkCommandBuffer cmd, const char* name, float r, float g, float b) {
    if (!pfn_vkCmdBeginDebugUtilsLabelEXT) return;
    
    VkDebugUtilsLabelEXT label = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pLabelName = name,
        .color = { r, g, b, 1.0f }
    };
    pfn_vkCmdBeginDebugUtilsLabelEXT(cmd, &label);
}

// End a debug label region
static void cmd_end_label(VkCommandBuffer cmd) {
    if (!pfn_vkCmdEndDebugUtilsLabelEXT) return;
    pfn_vkCmdEndDebugUtilsLabelEXT(cmd);
}

// Insert a debug marker
static void cmd_insert_label(VkCommandBuffer cmd, const char* name) {
    if (!pfn_vkCmdInsertDebugUtilsLabelEXT) return;
    
    VkDebugUtilsLabelEXT label = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pLabelName = name,
        .color = { 1.0f, 1.0f, 0.0f, 1.0f }
    };
    pfn_vkCmdInsertDebugUtilsLabelEXT(cmd, &label);
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    
    printf("==============================================\n");
    printf("  VKCompute - Episode 10: Debugging\n");
    printf("==============================================\n\n");
    
    // ========================================================================
    // Initialize with validation enabled
    // ========================================================================
    
    VkcContext ctx;
    VkcConfig config = vkc_config_default();
    config.app_name = "EP10 Debugging";
    config.enable_validation = true;  // Force validation on
    
    VK_CHECK(vkc_init(&ctx, &config));
    
    printf("Validation layers: %s\n", ctx.validation_enabled ? "ENABLED" : "DISABLED");
    
    // ========================================================================
    // Load debug utility functions
    // ========================================================================
    
    if (ctx.validation_enabled) {
        pfn_vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)
            vkGetDeviceProcAddr(ctx.device, "vkSetDebugUtilsObjectNameEXT");
        pfn_vkCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)
            vkGetDeviceProcAddr(ctx.device, "vkCmdBeginDebugUtilsLabelEXT");
        pfn_vkCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)
            vkGetDeviceProcAddr(ctx.device, "vkCmdEndDebugUtilsLabelEXT");
        pfn_vkCmdInsertDebugUtilsLabelEXT = (PFN_vkCmdInsertDebugUtilsLabelEXT)
            vkGetDeviceProcAddr(ctx.device, "vkCmdInsertDebugUtilsLabelEXT");
        
        printf("Debug utilities: %s\n", 
               pfn_vkSetDebugUtilsObjectNameEXT ? "AVAILABLE" : "NOT AVAILABLE");
    }
    
    // ========================================================================
    // Create resources with debug names
    // ========================================================================
    
    printf("\n=== Creating Named Resources ===\n");
    
    VkCommandPool cmd_pool;
    VK_CHECK(vkc_create_command_pool(&ctx, &cmd_pool));
    set_object_name(ctx.device, (uint64_t)cmd_pool, 
                    VK_OBJECT_TYPE_COMMAND_POOL, "MainComputeCommandPool");
    
    VkDeviceSize buffer_size = ARRAY_SIZE * sizeof(float);
    
    VkcBuffer input_buf, output_buf;
    VK_CHECK(vkc_create_buffer(&ctx, buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &input_buf));
    set_object_name(ctx.device, (uint64_t)input_buf.buffer,
                    VK_OBJECT_TYPE_BUFFER, "InputDataBuffer");
    set_object_name(ctx.device, (uint64_t)input_buf.memory,
                    VK_OBJECT_TYPE_DEVICE_MEMORY, "InputDataMemory");
    
    VK_CHECK(vkc_create_buffer(&ctx, buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &output_buf));
    set_object_name(ctx.device, (uint64_t)output_buf.buffer,
                    VK_OBJECT_TYPE_BUFFER, "OutputDataBuffer");
    
    printf("Named: CommandPool, Buffers, Memory\n");
    
    // Initialize data
    float* input_data = malloc(buffer_size);
    for (int i = 0; i < ARRAY_SIZE; i++) {
        input_data[i] = (float)i;
    }
    VK_CHECK(vkc_upload_buffer(&ctx, &input_buf, input_data, buffer_size));
    
    // ========================================================================
    // Create timestamp query pool for profiling
    // ========================================================================
    
    printf("\n=== Setting Up GPU Timestamps ===\n");
    
    // Check timestamp support
    float timestamp_period = ctx.device_props.limits.timestampPeriod;
    printf("Timestamp period: %.2f ns\n", timestamp_period);
    
    if (timestamp_period == 0) {
        printf("WARNING: Timestamps not supported on this device!\n");
    }
    
    VkQueryPoolCreateInfo query_pool_info = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = 4  // start, after first dispatch, after barrier, end
    };
    
    VkQueryPool timestamp_pool;
    VK_CHECK(vkCreateQueryPool(ctx.device, &query_pool_info, NULL, &timestamp_pool));
    set_object_name(ctx.device, (uint64_t)timestamp_pool,
                    VK_OBJECT_TYPE_QUERY_POOL, "TimestampQueryPool");
    
    // ========================================================================
    // Create pipeline
    // ========================================================================
    
    char* shader_path = vkc_path_relative_to_exe("shaders/ep10_debugging/compute.comp.spv");
    VkShaderModule shader;
    VK_CHECK(vkc_load_shader(&ctx, shader_path, &shader));
    free(shader_path);
    set_object_name(ctx.device, (uint64_t)shader,
                    VK_OBJECT_TYPE_SHADER_MODULE, "DoubleValueShader");
    
    VkDescriptorSetLayoutBinding bindings[] = {
        { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
        { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT }
    };
    
    VkDescriptorSetLayoutCreateInfo desc_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2, .pBindings = bindings
    };
    
    VkDescriptorSetLayout desc_layout;
    VK_CHECK(vkCreateDescriptorSetLayout(ctx.device, &desc_layout_info, NULL, &desc_layout));
    
    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1, .pSetLayouts = &desc_layout
    };
    
    VkPipelineLayout pipeline_layout;
    VK_CHECK(vkCreatePipelineLayout(ctx.device, &layout_info, NULL, &pipeline_layout));
    set_object_name(ctx.device, (uint64_t)pipeline_layout,
                    VK_OBJECT_TYPE_PIPELINE_LAYOUT, "ComputePipelineLayout");
    
    VkPipeline pipeline;
    VK_CHECK(vkc_create_compute_pipeline(&ctx, shader, pipeline_layout, &pipeline));
    set_object_name(ctx.device, (uint64_t)pipeline,
                    VK_OBJECT_TYPE_PIPELINE, "DoubleValuePipeline");
    
    // Descriptor pool and set
    VkDescriptorPoolSize pool_size = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &pool_size
    };
    
    VkDescriptorPool desc_pool;
    VK_CHECK(vkCreateDescriptorPool(ctx.device, &pool_info, NULL, &desc_pool));
    
    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = desc_pool, .descriptorSetCount = 1, .pSetLayouts = &desc_layout
    };
    
    VkDescriptorSet desc_set;
    VK_CHECK(vkAllocateDescriptorSets(ctx.device, &alloc_info, &desc_set));
    
    VkDescriptorBufferInfo buf_infos[] = {
        { input_buf.buffer, 0, VK_WHOLE_SIZE },
        { output_buf.buffer, 0, VK_WHOLE_SIZE }
    };
    
    VkWriteDescriptorSet writes[] = {
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = desc_set,
          .dstBinding = 0, .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buf_infos[0] },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = desc_set,
          .dstBinding = 1, .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buf_infos[1] }
    };
    vkUpdateDescriptorSets(ctx.device, 2, writes, 0, NULL);
    
    // ========================================================================
    // Execute with timestamps and debug labels
    // ========================================================================
    
    printf("\n=== Executing with Timestamps ===\n");
    
    VkCommandBuffer cmd;
    VK_CHECK(vkc_create_command_buffer(&ctx, cmd_pool, &cmd));
    set_object_name(ctx.device, (uint64_t)cmd,
                    VK_OBJECT_TYPE_COMMAND_BUFFER, "MainComputeCommandBuffer");
    
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
    
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
    
    // Reset timestamps
    vkCmdResetQueryPool(cmd, timestamp_pool, 0, 4);
    
    // Begin debug label region
    cmd_begin_label(cmd, "Compute Pass", 0.0f, 1.0f, 0.0f);  // Green
    
    // Timestamp: Start
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestamp_pool, 0);
    cmd_insert_label(cmd, "Start Timestamp");
    
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                            0, 1, &desc_set, 0, NULL);
    
    // First dispatch
    cmd_begin_label(cmd, "First Dispatch", 1.0f, 0.0f, 0.0f);  // Red
    vkCmdDispatch(cmd, vkc_div_ceil(ARRAY_SIZE, 256), 1, 1);
    cmd_end_label(cmd);
    
    // Timestamp: After first dispatch
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, timestamp_pool, 1);
    
    // Barrier
    VkMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 1, &barrier, 0, NULL, 0, NULL);
    
    // Timestamp: After barrier
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, timestamp_pool, 2);
    
    // Second dispatch (double again)
    cmd_begin_label(cmd, "Second Dispatch", 0.0f, 0.0f, 1.0f);  // Blue
    vkCmdDispatch(cmd, vkc_div_ceil(ARRAY_SIZE, 256), 1, 1);
    cmd_end_label(cmd);
    
    // Timestamp: End
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestamp_pool, 3);
    
    cmd_end_label(cmd);  // End "Compute Pass"
    
    VK_CHECK(vkEndCommandBuffer(cmd));
    
    double cpu_start = get_time_ms();
    VK_CHECK(vkc_submit_and_wait(&ctx, cmd));
    double cpu_time = get_time_ms() - cpu_start;
    
    // ========================================================================
    // Read timestamps
    // ========================================================================
    
    uint64_t timestamps[4];
    VK_CHECK(vkGetQueryPoolResults(ctx.device, timestamp_pool, 0, 4,
                                   sizeof(timestamps), timestamps, sizeof(uint64_t),
                                   VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
    
    printf("\n=== Timing Results ===\n");
    printf("CPU wall time: %.3f ms\n", cpu_time);
    
    if (timestamp_period > 0) {
        double total_gpu = (timestamps[3] - timestamps[0]) * timestamp_period / 1e6;
        double dispatch1 = (timestamps[1] - timestamps[0]) * timestamp_period / 1e6;
        double barrier_time = (timestamps[2] - timestamps[1]) * timestamp_period / 1e6;
        double dispatch2 = (timestamps[3] - timestamps[2]) * timestamp_period / 1e6;
        
        printf("GPU total time: %.3f ms\n", total_gpu);
        printf("  First dispatch: %.3f ms\n", dispatch1);
        printf("  Barrier: %.3f ms\n", barrier_time);
        printf("  Second dispatch: %.3f ms\n", dispatch2);
        
        double throughput = (ARRAY_SIZE * sizeof(float) * 2) / (total_gpu / 1000.0) / (1024.0 * 1024.0 * 1024.0);
        printf("Throughput: %.2f GB/s\n", throughput);
    }
    
    // ========================================================================
    // Verify results
    // ========================================================================
    
    float* output_data = malloc(buffer_size);
    VK_CHECK(vkc_download_buffer(&ctx, &output_buf, output_data, buffer_size));
    
    printf("\n=== Verification ===\n");
    printf("Expected: input * 4 (doubled twice)\n");
    
    bool success = true;
    for (int i = 0; i < 5; i++) {
        float expected = input_data[i] * 4.0f;
        printf("  [%d] %.1f -> %.1f (expected %.1f) %s\n",
               i, input_data[i], output_data[i], expected,
               output_data[i] == expected ? "✓" : "✗");
        if (output_data[i] != expected) success = false;
    }
    
    // ========================================================================
    // Demonstrate common errors (with fixes)
    // ========================================================================
    
    printf("\n=== Common Debugging Scenarios ===\n");
    printf("1. Validation layer messages appear above if there are issues\n");
    printf("2. Object names help identify resources in error messages\n");
    printf("3. Debug labels help track command buffer execution\n");
    printf("4. Timestamps pinpoint performance bottlenecks\n");
    
    printf("\nDebugging tips:\n");
    printf("  - Run with VK_LAYER_KHRONOS_validation for error detection\n");
    printf("  - Use RenderDoc for GPU debugging and capture\n");
    printf("  - Use NVIDIA Nsight or AMD RGP for profiling\n");
    printf("  - Name all objects for clearer error messages\n");
    printf("  - Add debug labels for easier trace navigation\n");
    
    // Cleanup
    free(input_data);
    free(output_data);
    
    vkDestroyQueryPool(ctx.device, timestamp_pool, NULL);
    vkDestroyPipeline(ctx.device, pipeline, NULL);
    vkDestroyPipelineLayout(ctx.device, pipeline_layout, NULL);
    vkDestroyDescriptorPool(ctx.device, desc_pool, NULL);
    vkDestroyDescriptorSetLayout(ctx.device, desc_layout, NULL);
    vkDestroyShaderModule(ctx.device, shader, NULL);
    vkDestroyCommandPool(ctx.device, cmd_pool, NULL);
    vkc_destroy_buffer(&ctx, &input_buf);
    vkc_destroy_buffer(&ctx, &output_buf);
    vkc_cleanup(&ctx);
    
    printf("\nEpisode 10 completed!\n");
    printf("\n=== Series Complete! ===\n");
    printf("You've learned the fundamentals of Vulkan Compute!\n");
    return 0;
}

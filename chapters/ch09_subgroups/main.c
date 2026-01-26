/**
 * VKCompute - Chapter 09: Subgroups
 * 
 * Learn about subgroup (wave/warp) operations for efficient parallel reductions.
 * 
 * Topics covered:
 * - What are subgroups (waves/warps)
 * - Querying subgroup properties
 * - Subgroup operations in GLSL
 * - Parallel reduction with subgroups
 * - Broadcast and shuffle operations
 * - Performance benefits
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "vk_init.h"
#include "vk_utils.h"
#include "file_utils.h"

#define ARRAY_SIZE (256 * 1024)  // 256K elements


int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    
    printf("==============================================\n");
    printf("  VKCompute - Chapter 09: Subgroups\n");
    printf("==============================================\n\n");
    
    VkcContext ctx;
    VK_CHECK(vkc_init(&ctx, NULL));
    
    // ========================================================================
    // Query subgroup properties
    // ========================================================================
    
    VkPhysicalDeviceSubgroupProperties subgroup_props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES
    };
    
    VkPhysicalDeviceProperties2 props2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &subgroup_props
    };
    
    vkGetPhysicalDeviceProperties2(ctx.physical_device, &props2);
    
    printf("=== Subgroup Properties ===\n");
    printf("  Subgroup Size: %u\n", subgroup_props.subgroupSize);
    printf("  Supported Stages: ");
    if (subgroup_props.supportedStages & VK_SHADER_STAGE_COMPUTE_BIT) printf("COMPUTE ");
    if (subgroup_props.supportedStages & VK_SHADER_STAGE_VERTEX_BIT) printf("VERTEX ");
    if (subgroup_props.supportedStages & VK_SHADER_STAGE_FRAGMENT_BIT) printf("FRAGMENT ");
    printf("\n");
    
    printf("  Supported Operations:\n");
    VkSubgroupFeatureFlags ops = subgroup_props.supportedOperations;
    if (ops & VK_SUBGROUP_FEATURE_BASIC_BIT) printf("    - Basic (elect, barrier)\n");
    if (ops & VK_SUBGROUP_FEATURE_VOTE_BIT) printf("    - Vote (all, any, equal)\n");
    if (ops & VK_SUBGROUP_FEATURE_ARITHMETIC_BIT) printf("    - Arithmetic (add, mul, min, max)\n");
    if (ops & VK_SUBGROUP_FEATURE_BALLOT_BIT) printf("    - Ballot\n");
    if (ops & VK_SUBGROUP_FEATURE_SHUFFLE_BIT) printf("    - Shuffle\n");
    if (ops & VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT) printf("    - Shuffle Relative\n");
    if (ops & VK_SUBGROUP_FEATURE_CLUSTERED_BIT) printf("    - Clustered\n");
    if (ops & VK_SUBGROUP_FEATURE_QUAD_BIT) printf("    - Quad\n");
    
    uint32_t subgroup_size = subgroup_props.subgroupSize;
    printf("\nThis GPU uses %u-wide subgroups (like CUDA warps of %u threads)\n", 
           subgroup_size, subgroup_size);
    
    // Check if arithmetic operations are supported
    if (!(ops & VK_SUBGROUP_FEATURE_ARITHMETIC_BIT)) {
        printf("\nWARNING: Subgroup arithmetic not supported on this device!\n");
        printf("Some examples may not work correctly.\n");
    }
    
    VkCommandPool cmd_pool;
    VK_CHECK(vkc_create_command_pool(&ctx, &cmd_pool));
    
    // ========================================================================
    // Create buffers
    // ========================================================================
    
    VkDeviceSize buffer_size = ARRAY_SIZE * sizeof(float);
    VkDeviceSize workgroup_count = ARRAY_SIZE / 256;  // Using 256 workgroup size
    VkDeviceSize partial_size = workgroup_count * sizeof(float);
    
    VkcBuffer input_buf, partial_buf, output_buf;
    VK_CHECK(vkc_create_buffer(&ctx, buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &input_buf));
    VK_CHECK(vkc_create_buffer(&ctx, partial_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &partial_buf));
    VK_CHECK(vkc_create_buffer(&ctx, sizeof(float), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &output_buf));
    
    // Initialize with values 1..N
    float* input_data = malloc(buffer_size);
    double expected_sum = 0;
    for (int i = 0; i < ARRAY_SIZE; i++) {
        input_data[i] = 1.0f;  // Simple: sum of 1s = N
        expected_sum += input_data[i];
    }
    VK_CHECK(vkc_upload_buffer(&ctx, &input_buf, input_data, buffer_size));
    
    printf("\n=== Test Data ===\n");
    printf("Array size: %d elements\n", ARRAY_SIZE);
    printf("Expected sum: %.0f\n", expected_sum);
    
    // ========================================================================
    // Create pipeline for subgroup reduction
    // ========================================================================
    
    char* shader_path = vkc_path_relative_to_exe("shaders/ch09_subgroups/reduce.comp.spv");
    VkShaderModule shader;
    VK_CHECK(vkc_load_shader(&ctx, shader_path, &shader));
    free(shader_path);
    
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
    
    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = sizeof(uint32_t)
    };
    
    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1, .pSetLayouts = &desc_layout,
        .pushConstantRangeCount = 1, .pPushConstantRanges = &push_range
    };
    
    VkPipelineLayout pipeline_layout;
    VK_CHECK(vkCreatePipelineLayout(ctx.device, &layout_info, NULL, &pipeline_layout));
    
    VkPipeline pipeline;
    VK_CHECK(vkc_create_compute_pipeline(&ctx, shader, pipeline_layout, &pipeline));
    
    // Descriptor pool and sets
    VkDescriptorPoolSize pool_size = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4 };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 2, .poolSizeCount = 1, .pPoolSizes = &pool_size
    };
    
    VkDescriptorPool desc_pool;
    VK_CHECK(vkCreateDescriptorPool(ctx.device, &pool_info, NULL, &desc_pool));
    
    VkDescriptorSetLayout layouts[] = { desc_layout, desc_layout };
    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = desc_pool, .descriptorSetCount = 2, .pSetLayouts = layouts
    };
    
    VkDescriptorSet desc_sets[2];
    VK_CHECK(vkAllocateDescriptorSets(ctx.device, &alloc_info, desc_sets));
    
    // Set 0: input -> partial
    VkDescriptorBufferInfo buf_infos0[] = {
        { input_buf.buffer, 0, VK_WHOLE_SIZE },
        { partial_buf.buffer, 0, VK_WHOLE_SIZE }
    };
    VkWriteDescriptorSet writes0[] = {
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = desc_sets[0],
          .dstBinding = 0, .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buf_infos0[0] },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = desc_sets[0],
          .dstBinding = 1, .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buf_infos0[1] }
    };
    vkUpdateDescriptorSets(ctx.device, 2, writes0, 0, NULL);
    
    // Set 1: partial -> output
    VkDescriptorBufferInfo buf_infos1[] = {
        { partial_buf.buffer, 0, VK_WHOLE_SIZE },
        { output_buf.buffer, 0, VK_WHOLE_SIZE }
    };
    VkWriteDescriptorSet writes1[] = {
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = desc_sets[1],
          .dstBinding = 0, .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buf_infos1[0] },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = desc_sets[1],
          .dstBinding = 1, .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buf_infos1[1] }
    };
    vkUpdateDescriptorSets(ctx.device, 2, writes1, 0, NULL);
    
    // ========================================================================
    // Run reduction
    // ========================================================================
    
    printf("\n=== Running Subgroup Reduction ===\n");
    
    VkCommandBuffer cmd;
    VK_CHECK(vkc_create_command_buffer(&ctx, cmd_pool, &cmd));
    
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    
    // Two-pass reduction
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
    
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    
    // Pass 1: Reduce ARRAY_SIZE -> workgroup_count partial sums
    uint32_t count = ARRAY_SIZE;
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                            0, 1, &desc_sets[0], 0, NULL);
    vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &count);
    vkCmdDispatch(cmd, (uint32_t)workgroup_count, 1, 1);
    
    printf("Pass 1: %d elements -> %lu partial sums\n", ARRAY_SIZE, (unsigned long)workgroup_count);
    
    // Barrier
    VkMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 1, &barrier, 0, NULL, 0, NULL);
    
    // Pass 2: Reduce partial sums -> final result
    count = (uint32_t)workgroup_count;
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                            0, 1, &desc_sets[1], 0, NULL);
    vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &count);
    vkCmdDispatch(cmd, 1, 1, 1);  // Single workgroup for final reduction
    
    printf("Pass 2: %u partial sums -> 1 final result\n", count);
    
    VK_CHECK(vkEndCommandBuffer(cmd));
    
    double start = vkc_get_time_ms();
    VK_CHECK(vkc_submit_and_wait(&ctx, cmd));
    double elapsed = vkc_get_time_ms() - start;
    
    // Read result
    float result;
    VK_CHECK(vkc_download_buffer(&ctx, &output_buf, &result, sizeof(float)));
    
    printf("\n=== Results ===\n");
    printf("Computed sum: %.0f\n", result);
    printf("Expected sum: %.0f\n", expected_sum);
    printf("Match: %s\n", (result == expected_sum) ? "✓ YES" : "✗ NO");
    printf("Time: %.3f ms\n", elapsed);
    
    // ========================================================================
    // Benchmark
    // ========================================================================
    
    printf("\n=== Benchmark ===\n");
    
    int iterations = 1000;
    start = vkc_get_time_ms();
    
    for (int i = 0; i < iterations; i++) {
        VK_CHECK(vkResetCommandBuffer(cmd, 0));
        VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
        
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        
        count = ARRAY_SIZE;
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                                0, 1, &desc_sets[0], 0, NULL);
        vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &count);
        vkCmdDispatch(cmd, (uint32_t)workgroup_count, 1, 1);
        
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0, 1, &barrier, 0, NULL, 0, NULL);
        
        count = (uint32_t)workgroup_count;
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                                0, 1, &desc_sets[1], 0, NULL);
        vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &count);
        vkCmdDispatch(cmd, 1, 1, 1);
        
        VK_CHECK(vkEndCommandBuffer(cmd));
        VK_CHECK(vkc_submit_and_wait(&ctx, cmd));
    }
    
    elapsed = vkc_get_time_ms() - start;
    double per_iter = elapsed / iterations;
    double throughput = (ARRAY_SIZE * sizeof(float)) / (per_iter / 1000.0) / (1024.0 * 1024.0 * 1024.0);
    
    printf("Iterations: %d\n", iterations);
    printf("Time per reduction: %.4f ms\n", per_iter);
    printf("Throughput: %.2f GB/s\n", throughput);
    printf("Elements/second: %.2f billion\n", (ARRAY_SIZE / (per_iter / 1000.0)) / 1e9);
    
    // Cleanup
    free(input_data);
    
    vkDestroyPipeline(ctx.device, pipeline, NULL);
    vkDestroyPipelineLayout(ctx.device, pipeline_layout, NULL);
    vkDestroyDescriptorPool(ctx.device, desc_pool, NULL);
    vkDestroyDescriptorSetLayout(ctx.device, desc_layout, NULL);
    vkDestroyShaderModule(ctx.device, shader, NULL);
    vkDestroyCommandPool(ctx.device, cmd_pool, NULL);
    vkc_destroy_buffer(&ctx, &input_buf);
    vkc_destroy_buffer(&ctx, &partial_buf);
    vkc_destroy_buffer(&ctx, &output_buf);
    vkc_cleanup(&ctx);
    
    printf("\nChapter 09 completed!\n");
    return 0;
}

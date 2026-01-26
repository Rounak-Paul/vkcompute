/**
 * VKCompute - Chapter 05: Synchronization
 * 
 * Master GPU synchronization primitives for correct and efficient compute.
 * 
 * Topics covered:
 * - Fences (CPU-GPU synchronization)
 * - Semaphores (GPU-GPU synchronization)
 * - Pipeline barriers (execution and memory barriers)
 * - Events for fine-grained synchronization
 * - Multiple queue submission patterns
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "vk_init.h"
#include "vk_utils.h"
#include "file_utils.h"

#define ARRAY_SIZE 1024
#define NUM_ITERATIONS 5


int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    
    printf("==============================================\n");
    printf("  VKCompute - Chapter 05: Synchronization\n");
    printf("==============================================\n\n");
    
    VkcContext ctx;
    VK_CHECK(vkc_init(&ctx, NULL));
    
    VkCommandPool cmd_pool;
    VK_CHECK(vkc_create_command_pool(&ctx, &cmd_pool));
    
    VkDeviceSize buffer_size = ARRAY_SIZE * sizeof(float);
    
    // Create buffers
    VkcBuffer buf_a, buf_b, buf_c;
    VK_CHECK(vkc_create_buffer(&ctx, buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &buf_a));
    VK_CHECK(vkc_create_buffer(&ctx, buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &buf_b));
    VK_CHECK(vkc_create_buffer(&ctx, buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &buf_c));
    
    // Initialize data
    float* data = malloc(buffer_size);
    for (int i = 0; i < ARRAY_SIZE; i++) data[i] = (float)i;
    VK_CHECK(vkc_upload_buffer(&ctx, &buf_a, data, buffer_size));
    
    // Load shader
    char* shader_path = vkc_path_relative_to_exe("shaders/ch05_synchronization/increment.comp.spv");
    VkShaderModule shader;
    VK_CHECK(vkc_load_shader(&ctx, shader_path, &shader));
    free(shader_path);
    
    // Create pipeline
    VkDescriptorSetLayoutBinding binding = {
        .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
    };
    
    VkDescriptorSetLayoutCreateInfo desc_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1, .pBindings = &binding
    };
    
    VkDescriptorSetLayout desc_layout;
    VK_CHECK(vkCreateDescriptorSetLayout(ctx.device, &desc_layout_info, NULL, &desc_layout));
    
    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1, .pSetLayouts = &desc_layout
    };
    
    VkPipelineLayout pipeline_layout;
    VK_CHECK(vkCreatePipelineLayout(ctx.device, &layout_info, NULL, &pipeline_layout));
    
    VkPipeline pipeline;
    VK_CHECK(vkc_create_compute_pipeline(&ctx, shader, pipeline_layout, &pipeline));
    
    // Descriptor pool
    VkDescriptorPoolSize pool_size = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 3, .poolSizeCount = 1, .pPoolSizes = &pool_size
    };
    
    VkDescriptorPool desc_pool;
    VK_CHECK(vkCreateDescriptorPool(ctx.device, &pool_info, NULL, &desc_pool));
    
    // Allocate descriptor sets for each buffer
    VkDescriptorSetLayout layouts[3] = { desc_layout, desc_layout, desc_layout };
    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = desc_pool, .descriptorSetCount = 3, .pSetLayouts = layouts
    };
    
    VkDescriptorSet desc_sets[3];
    VK_CHECK(vkAllocateDescriptorSets(ctx.device, &alloc_info, desc_sets));
    
    VkBuffer buffers[3] = { buf_a.buffer, buf_b.buffer, buf_c.buffer };
    for (int i = 0; i < 3; i++) {
        VkDescriptorBufferInfo buf_info = { buffers[i], 0, VK_WHOLE_SIZE };
        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = desc_sets[i], .dstBinding = 0, .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buf_info
        };
        vkUpdateDescriptorSets(ctx.device, 1, &write, 0, NULL);
    }
    
    // ========================================================================
    // Part 1: Fences - CPU-GPU synchronization
    // ========================================================================
    
    printf("=== Part 1: Fences (CPU-GPU Sync) ===\n");
    
    VkFence fence;
    VK_CHECK(vkc_create_fence(&ctx, &fence, false));
    
    VkCommandBuffer cmd;
    VK_CHECK(vkc_create_command_buffer(&ctx, cmd_pool, &cmd));
    
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
    
    // Submit work and wait with fence
    printf("Submitting work and waiting with fence...\n");
    double start = vkc_get_time_ms();
    
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                            0, 1, &desc_sets[0], 0, NULL);
    vkCmdDispatch(cmd, vkc_div_ceil(ARRAY_SIZE, 256), 1, 1);
    VK_CHECK(vkEndCommandBuffer(cmd));
    
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1, .pCommandBuffers = &cmd
    };
    
    VK_CHECK(vkQueueSubmit(ctx.compute_queue, 1, &submit_info, fence));
    
    // Fence wait options demo
    VkResult wait_result = vkWaitForFences(ctx.device, 1, &fence, VK_TRUE, 0);
    printf("  Immediate check: %s\n", wait_result == VK_SUCCESS ? "Complete" : "Not ready");
    
    VK_CHECK(vkWaitForFences(ctx.device, 1, &fence, VK_TRUE, UINT64_MAX));
    printf("  Fence signaled after %.2f ms\n", vkc_get_time_ms() - start);
    
    VK_CHECK(vkc_download_buffer(&ctx, &buf_a, data, buffer_size));
    printf("  Result: data[0] = %.0f (was 0, incremented by 1)\n", data[0]);
    
    // Reset fence for reuse
    VK_CHECK(vkResetFences(ctx.device, 1, &fence));
    
    // ========================================================================
    // Part 2: Pipeline Barriers - Execution dependencies
    // ========================================================================
    
    printf("\n=== Part 2: Pipeline Barriers ===\n");
    printf("Running chained dispatches with barriers...\n");
    
    // Reset buffer
    for (int i = 0; i < ARRAY_SIZE; i++) data[i] = 0;
    VK_CHECK(vkc_upload_buffer(&ctx, &buf_a, data, buffer_size));
    
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
    
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                            0, 1, &desc_sets[0], 0, NULL);
    
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        vkCmdDispatch(cmd, vkc_div_ceil(ARRAY_SIZE, 256), 1, 1);
        
        if (i < NUM_ITERATIONS - 1) {
            // Memory barrier ensures writes are visible before next read
            VkMemoryBarrier barrier = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
            };
            
            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // src stage
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // dst stage
                0, 1, &barrier, 0, NULL, 0, NULL);
        }
    }
    
    VK_CHECK(vkEndCommandBuffer(cmd));
    
    start = vkc_get_time_ms();
    VK_CHECK(vkQueueSubmit(ctx.compute_queue, 1, &submit_info, fence));
    VK_CHECK(vkWaitForFences(ctx.device, 1, &fence, VK_TRUE, UINT64_MAX));
    printf("  %d chained dispatches completed in %.2f ms\n", NUM_ITERATIONS, vkc_get_time_ms() - start);
    
    VK_CHECK(vkc_download_buffer(&ctx, &buf_a, data, buffer_size));
    printf("  Result: data[0] = %.0f (expected %d)\n", data[0], NUM_ITERATIONS);
    
    // ========================================================================
    // Part 3: Buffer Memory Barriers - More specific synchronization
    // ========================================================================
    
    printf("\n=== Part 3: Buffer Memory Barriers ===\n");
    
    // Initialize buffers
    for (int i = 0; i < ARRAY_SIZE; i++) data[i] = (float)i;
    VK_CHECK(vkc_upload_buffer(&ctx, &buf_a, data, buffer_size));
    memset(data, 0, buffer_size);
    VK_CHECK(vkc_upload_buffer(&ctx, &buf_b, data, buffer_size));
    
    VK_CHECK(vkResetFences(ctx.device, 1, &fence));
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
    
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    
    // Increment buf_a
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                            0, 1, &desc_sets[0], 0, NULL);
    vkCmdDispatch(cmd, vkc_div_ceil(ARRAY_SIZE, 256), 1, 1);
    
    // Buffer-specific barrier for buf_a
    VkBufferMemoryBarrier buf_barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = buf_a.buffer,
        .offset = 0,
        .size = VK_WHOLE_SIZE
    };
    
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, NULL, 1, &buf_barrier, 0, NULL);
    
    // Now increment buf_b (independent, but after buf_a is done)
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                            0, 1, &desc_sets[1], 0, NULL);
    vkCmdDispatch(cmd, vkc_div_ceil(ARRAY_SIZE, 256), 1, 1);
    
    VK_CHECK(vkEndCommandBuffer(cmd));
    VK_CHECK(vkQueueSubmit(ctx.compute_queue, 1, &submit_info, fence));
    VK_CHECK(vkWaitForFences(ctx.device, 1, &fence, VK_TRUE, UINT64_MAX));
    
    float* result_a = malloc(buffer_size);
    float* result_b = malloc(buffer_size);
    VK_CHECK(vkc_download_buffer(&ctx, &buf_a, result_a, buffer_size));
    VK_CHECK(vkc_download_buffer(&ctx, &buf_b, result_b, buffer_size));
    
    printf("  buf_a[0] = %.0f (was 0, +1)\n", result_a[0]);
    printf("  buf_a[5] = %.0f (was 5, +1)\n", result_a[5]);
    printf("  buf_b[0] = %.0f (was 0, +1)\n", result_b[0]);
    
    // ========================================================================
    // Part 4: Multiple Submissions with Fences
    // ========================================================================
    
    printf("\n=== Part 4: Multiple Submissions ===\n");
    
    // Create multiple command buffers and fences
    VkCommandBuffer cmds[3];
    VkFence fences[3];
    
    VkCommandBufferAllocateInfo cmd_alloc = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 3
    };
    VK_CHECK(vkAllocateCommandBuffers(ctx.device, &cmd_alloc, cmds));
    
    for (int i = 0; i < 3; i++) {
        VK_CHECK(vkc_create_fence(&ctx, &fences[i], false));
    }
    
    // Reset all buffers
    for (int i = 0; i < ARRAY_SIZE; i++) data[i] = 0;
    VK_CHECK(vkc_upload_buffer(&ctx, &buf_a, data, buffer_size));
    VK_CHECK(vkc_upload_buffer(&ctx, &buf_b, data, buffer_size));
    VK_CHECK(vkc_upload_buffer(&ctx, &buf_c, data, buffer_size));
    
    // Record and submit 3 independent operations
    printf("Submitting 3 independent operations...\n");
    start = vkc_get_time_ms();
    
    for (int i = 0; i < 3; i++) {
        VK_CHECK(vkBeginCommandBuffer(cmds[i], &begin_info));
        vkCmdBindPipeline(cmds[i], VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(cmds[i], VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                                0, 1, &desc_sets[i], 0, NULL);
        
        // Each buffer gets different number of increments
        for (int j = 0; j <= i; j++) {
            vkCmdDispatch(cmds[i], vkc_div_ceil(ARRAY_SIZE, 256), 1, 1);
            if (j < i) {
                VkMemoryBarrier barrier = {
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
                };
                vkCmdPipelineBarrier(cmds[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                     0, 1, &barrier, 0, NULL, 0, NULL);
            }
        }
        
        VK_CHECK(vkEndCommandBuffer(cmds[i]));
        
        VkSubmitInfo sub = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1, .pCommandBuffers = &cmds[i]
        };
        VK_CHECK(vkQueueSubmit(ctx.compute_queue, 1, &sub, fences[i]));
    }
    
    // Wait for all fences
    VK_CHECK(vkWaitForFences(ctx.device, 3, fences, VK_TRUE, UINT64_MAX));
    printf("  All completed in %.2f ms\n", vkc_get_time_ms() - start);
    
    VK_CHECK(vkc_download_buffer(&ctx, &buf_a, result_a, buffer_size));
    VK_CHECK(vkc_download_buffer(&ctx, &buf_b, result_b, buffer_size));
    float* result_c = malloc(buffer_size);
    VK_CHECK(vkc_download_buffer(&ctx, &buf_c, result_c, buffer_size));
    
    printf("  buf_a[0] = %.0f (1 dispatch)\n", result_a[0]);
    printf("  buf_b[0] = %.0f (2 dispatches)\n", result_b[0]);
    printf("  buf_c[0] = %.0f (3 dispatches)\n", result_c[0]);
    
    // Cleanup
    free(data);
    free(result_a);
    free(result_b);
    free(result_c);
    
    for (int i = 0; i < 3; i++) {
        vkDestroyFence(ctx.device, fences[i], NULL);
    }
    vkDestroyFence(ctx.device, fence, NULL);
    vkDestroyPipeline(ctx.device, pipeline, NULL);
    vkDestroyPipelineLayout(ctx.device, pipeline_layout, NULL);
    vkDestroyDescriptorPool(ctx.device, desc_pool, NULL);
    vkDestroyDescriptorSetLayout(ctx.device, desc_layout, NULL);
    vkDestroyShaderModule(ctx.device, shader, NULL);
    vkDestroyCommandPool(ctx.device, cmd_pool, NULL);
    vkc_destroy_buffer(&ctx, &buf_a);
    vkc_destroy_buffer(&ctx, &buf_b);
    vkc_destroy_buffer(&ctx, &buf_c);
    vkc_cleanup(&ctx);
    
    printf("\nChapter 05 completed!\n");
    return 0;
}

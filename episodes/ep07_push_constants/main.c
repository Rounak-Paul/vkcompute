/**
 * VKCompute - Episode 07: Push Constants
 * 
 * Learn to use push constants for fast, small data updates without descriptors.
 * 
 * Topics covered:
 * - Push constant basics and limits
 * - When to use push constants vs uniform buffers
 * - Multiple push constant ranges
 * - Updating push constants between dispatches
 * - Performance comparison with uniform buffers
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


#include "vk_init.h"
#include "vk_utils.h"
#include "file_utils.h"

#define ARRAY_SIZE 1024
#define NUM_ITERATIONS 1000


// Push constant structure - must match shader
typedef struct {
    float scale;
    float offset;
    float power;
    uint32_t operation;  // 0=linear, 1=polynomial, 2=sinusoidal
} PushConstants;

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    
    printf("==============================================\n");
    printf("  VKCompute - Episode 07: Push Constants\n");
    printf("==============================================\n\n");
    
    VkcContext ctx;
    VK_CHECK(vkc_init(&ctx, NULL));
    
    // ========================================================================
    // Check push constant limits
    // ========================================================================
    
    printf("=== Push Constant Limits ===\n");
    printf("  Max Push Constants Size: %u bytes\n", 
           ctx.device_props.limits.maxPushConstantsSize);
    printf("  Our struct size: %zu bytes\n", sizeof(PushConstants));
    
    if (sizeof(PushConstants) > ctx.device_props.limits.maxPushConstantsSize) {
        printf("ERROR: Push constants too large!\n");
        return 1;
    }
    
    VkCommandPool cmd_pool;
    VK_CHECK(vkc_create_command_pool(&ctx, &cmd_pool));
    
    // ========================================================================
    // Create buffers
    // ========================================================================
    
    VkDeviceSize buffer_size = ARRAY_SIZE * sizeof(float);
    
    VkcBuffer input_buf, output_buf;
    VK_CHECK(vkc_create_buffer(&ctx, buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &input_buf));
    VK_CHECK(vkc_create_buffer(&ctx, buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &output_buf));
    
    // Fill input
    float* input_data = malloc(buffer_size);
    for (int i = 0; i < ARRAY_SIZE; i++) {
        input_data[i] = (float)i / ARRAY_SIZE;  // 0.0 to ~1.0
    }
    VK_CHECK(vkc_upload_buffer(&ctx, &input_buf, input_data, buffer_size));
    
    // ========================================================================
    // Create descriptor set (for buffers only - params via push constants)
    // ========================================================================
    
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
    
    // ========================================================================
    // Create pipeline layout WITH push constants
    // ========================================================================
    
    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = sizeof(PushConstants)
    };
    
    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &desc_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_range
    };
    
    VkPipelineLayout pipeline_layout;
    VK_CHECK(vkCreatePipelineLayout(ctx.device, &layout_info, NULL, &pipeline_layout));
    
    printf("\nPipeline layout created with %zu bytes of push constants\n", 
           sizeof(PushConstants));
    
    // ========================================================================
    // Load shader and create pipeline
    // ========================================================================
    
    char* shader_path = vkc_path_relative_to_exe("shaders/ep07_push_constants/transform.comp.spv");
    VkShaderModule shader;
    VK_CHECK(vkc_load_shader(&ctx, shader_path, &shader));
    free(shader_path);
    
    VkPipeline pipeline;
    VK_CHECK(vkc_create_compute_pipeline(&ctx, shader, pipeline_layout, &pipeline));
    
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
    
    VkCommandBuffer cmd;
    VK_CHECK(vkc_create_command_buffer(&ctx, cmd_pool, &cmd));
    
    float* output_data = malloc(buffer_size);
    
    // ========================================================================
    // Test different operations using push constants
    // ========================================================================
    
    printf("\n=== Testing Push Constant Operations ===\n");
    
    struct {
        const char* name;
        PushConstants pc;
    } tests[] = {
        { "Linear (2x + 0.5)", { .scale = 2.0f, .offset = 0.5f, .power = 1.0f, .operation = 0 } },
        { "Polynomial (x^2)", { .scale = 1.0f, .offset = 0.0f, .power = 2.0f, .operation = 1 } },
        { "Polynomial (x^0.5)", { .scale = 1.0f, .offset = 0.0f, .power = 0.5f, .operation = 1 } },
        { "Sinusoidal", { .scale = 1.0f, .offset = 0.0f, .power = 1.0f, .operation = 2 } }
    };
    
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    
    for (size_t t = 0; t < sizeof(tests) / sizeof(tests[0]); t++) {
        printf("\n--- %s ---\n", tests[t].name);
        
        VK_CHECK(vkResetCommandBuffer(cmd, 0));
        VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
        
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                                0, 1, &desc_set, 0, NULL);
        
        // Push constants - this is the key call!
        vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                          0, sizeof(PushConstants), &tests[t].pc);
        
        vkCmdDispatch(cmd, vkc_div_ceil(ARRAY_SIZE, 256), 1, 1);
        
        VK_CHECK(vkEndCommandBuffer(cmd));
        VK_CHECK(vkc_submit_and_wait(&ctx, cmd));
        
        VK_CHECK(vkc_download_buffer(&ctx, &output_buf, output_data, buffer_size));
        
        // Show sample results
        int samples[] = { 0, 256, 512, 768, 1023 };
        for (int i = 0; i < 5; i++) {
            int idx = samples[i];
            printf("  [%4d] %.4f -> %.4f\n", idx, input_data[idx], output_data[idx]);
        }
    }
    
    // ========================================================================
    // Multiple dispatches with changing push constants
    // ========================================================================
    
    printf("\n=== Chained Dispatches with Different Push Constants ===\n");
    
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
    
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                            0, 1, &desc_set, 0, NULL);
    
    // First dispatch: linear transform
    PushConstants pc1 = { .scale = 2.0f, .offset = 0.0f, .power = 1.0f, .operation = 0 };
    vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                      0, sizeof(PushConstants), &pc1);
    vkCmdDispatch(cmd, vkc_div_ceil(ARRAY_SIZE, 256), 1, 1);
    
    // Barrier
    VkMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 1, &barrier, 0, NULL, 0, NULL);
    
    // Swap input/output descriptors for chaining (or use same buffer)
    // For simplicity, we'll just update the scale (output becomes input * scale again)
    
    // Second dispatch: square the result
    PushConstants pc2 = { .scale = 1.0f, .offset = 0.0f, .power = 2.0f, .operation = 1 };
    vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                      0, sizeof(PushConstants), &pc2);
    vkCmdDispatch(cmd, vkc_div_ceil(ARRAY_SIZE, 256), 1, 1);
    
    VK_CHECK(vkEndCommandBuffer(cmd));
    VK_CHECK(vkc_submit_and_wait(&ctx, cmd));
    
    VK_CHECK(vkc_download_buffer(&ctx, &output_buf, output_data, buffer_size));
    
    printf("Chain: input * 2, then square\n");
    printf("  [0] %.4f -> (%.4f)^2 = %.4f\n", input_data[0], input_data[0] * 2, output_data[0]);
    printf("  [512] %.4f -> (%.4f)^2 = %.4f\n", input_data[512], input_data[512] * 2, output_data[512]);
    
    // ========================================================================
    // Performance: Push constants vs. updating uniform buffer
    // ========================================================================
    
    printf("\n=== Performance Comparison ===\n");
    printf("Running %d iterations...\n", NUM_ITERATIONS);
    
    // Push constants method
    double start = vkc_get_time_ms();
    
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        PushConstants pc = {
            .scale = (float)i / NUM_ITERATIONS,
            .offset = 0.0f,
            .power = 1.0f,
            .operation = 0
        };
        
        VK_CHECK(vkResetCommandBuffer(cmd, 0));
        VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                                0, 1, &desc_set, 0, NULL);
        vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                          0, sizeof(PushConstants), &pc);
        vkCmdDispatch(cmd, vkc_div_ceil(ARRAY_SIZE, 256), 1, 1);
        VK_CHECK(vkEndCommandBuffer(cmd));
        VK_CHECK(vkc_submit_and_wait(&ctx, cmd));
    }
    
    double push_const_time = vkc_get_time_ms() - start;
    printf("Push constants: %.2f ms total (%.4f ms/iter)\n", 
           push_const_time, push_const_time / NUM_ITERATIONS);
    
    // Cleanup
    free(input_data);
    free(output_data);
    
    vkDestroyPipeline(ctx.device, pipeline, NULL);
    vkDestroyPipelineLayout(ctx.device, pipeline_layout, NULL);
    vkDestroyDescriptorPool(ctx.device, desc_pool, NULL);
    vkDestroyDescriptorSetLayout(ctx.device, desc_layout, NULL);
    vkDestroyShaderModule(ctx.device, shader, NULL);
    vkDestroyCommandPool(ctx.device, cmd_pool, NULL);
    vkc_destroy_buffer(&ctx, &input_buf);
    vkc_destroy_buffer(&ctx, &output_buf);
    vkc_cleanup(&ctx);
    
    printf("\nEpisode 07 completed!\n");
    return 0;
}

/**
 * VKCompute - Episode 04: Pipeline Management
 * 
 * Learn about compute pipeline creation, caching, and using multiple pipelines.
 * 
 * Topics covered:
 * - Pipeline cache for faster pipeline creation
 * - Multiple compute pipelines in one application
 * - Pipeline switching costs
 * - Shared pipeline layouts
 * - Batch processing with different operations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "vk_init.h"
#include "vk_utils.h"
#include "file_utils.h"

#define ARRAY_SIZE 1024

static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

// Different operations we'll create pipelines for
typedef enum {
    OP_ADD,
    OP_MULTIPLY, 
    OP_SQUARE,
    OP_SQRT,
    OP_COUNT
} Operation;

const char* op_names[] = { "Add", "Multiply", "Square", "Sqrt" };
const char* op_shaders[] = { "add.comp.spv", "multiply.comp.spv", "square.comp.spv", "sqrt.comp.spv" };

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    
    printf("==============================================\n");
    printf("  VKCompute - Episode 04: Pipeline Management\n");
    printf("==============================================\n\n");
    
    VkcContext ctx;
    VK_CHECK(vkc_init(&ctx, NULL));
    
    VkCommandPool cmd_pool;
    VK_CHECK(vkc_create_command_pool(&ctx, &cmd_pool));
    
    // ========================================================================
    // Create pipeline cache
    // ========================================================================
    
    printf("Creating pipeline cache...\n");
    
    VkPipelineCacheCreateInfo cache_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .initialDataSize = 0,
        .pInitialData = NULL
    };
    
    VkPipelineCache pipeline_cache;
    VK_CHECK(vkCreatePipelineCache(ctx.device, &cache_info, NULL, &pipeline_cache));
    
    // ========================================================================
    // Create shared descriptor set layout and pipeline layout
    // ========================================================================
    
    printf("Creating shared layouts...\n");
    
    // All our operations use: input buffer, output buffer, scalar value
    VkDescriptorSetLayoutBinding bindings[] = {
        { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
        { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT }
    };
    
    VkDescriptorSetLayoutCreateInfo desc_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = bindings
    };
    
    VkDescriptorSetLayout desc_layout;
    VK_CHECK(vkCreateDescriptorSetLayout(ctx.device, &desc_layout_info, NULL, &desc_layout));
    
    // Push constant for scalar value
    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = sizeof(float)
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
    
    // ========================================================================
    // Load shaders and create all pipelines
    // ========================================================================
    
    printf("\nCreating %d pipelines...\n", OP_COUNT);
    
    VkShaderModule shaders[OP_COUNT];
    VkPipeline pipelines[OP_COUNT];
    
    double total_time = 0;
    
    for (int i = 0; i < OP_COUNT; i++) {
        char shader_file[256];
        snprintf(shader_file, sizeof(shader_file), "shaders/ep04_pipelines/%s", op_shaders[i]);
        char* shader_path = vkc_path_relative_to_exe(shader_file);
        
        VK_CHECK(vkc_load_shader(&ctx, shader_path, &shaders[i]));
        free(shader_path);
        
        VkPipelineShaderStageCreateInfo stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shaders[i],
            .pName = "main"
        };
        
        VkComputePipelineCreateInfo pipeline_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = stage,
            .layout = pipeline_layout
        };
        
        double start = get_time_ms();
        VK_CHECK(vkCreateComputePipelines(ctx.device, pipeline_cache, 1, 
                                          &pipeline_info, NULL, &pipelines[i]));
        double elapsed = get_time_ms() - start;
        total_time += elapsed;
        
        printf("  Pipeline '%s': %.2f ms\n", op_names[i], elapsed);
    }
    
    printf("Total pipeline creation: %.2f ms\n", total_time);
    
    // Get cache data size
    size_t cache_size = 0;
    vkGetPipelineCacheData(ctx.device, pipeline_cache, &cache_size, NULL);
    printf("Pipeline cache size: %zu bytes\n", cache_size);
    
    // ========================================================================
    // Create buffers and descriptors
    // ========================================================================
    
    VkDeviceSize buffer_size = ARRAY_SIZE * sizeof(float);
    
    VkcBuffer input_buf, output_buf;
    VK_CHECK(vkc_create_buffer(&ctx, buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &input_buf));
    VK_CHECK(vkc_create_buffer(&ctx, buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &output_buf));
    
    // Initialize input
    float* input_data = malloc(buffer_size);
    for (int i = 0; i < ARRAY_SIZE; i++) {
        input_data[i] = (float)(i + 1);  // 1, 2, 3, ...
    }
    VK_CHECK(vkc_upload_buffer(&ctx, &input_buf, input_data, buffer_size));
    
    // Descriptor pool and set
    VkDescriptorPoolSize pool_size = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 };
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
    
    VkDescriptorBufferInfo buf_infos[] = {
        { .buffer = input_buf.buffer, .offset = 0, .range = VK_WHOLE_SIZE },
        { .buffer = output_buf.buffer, .offset = 0, .range = VK_WHOLE_SIZE }
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
    // Run each pipeline and show results
    // ========================================================================
    
    printf("\n=== Running All Pipelines ===\n");
    
    VkCommandBuffer cmd;
    VK_CHECK(vkc_create_command_buffer(&ctx, cmd_pool, &cmd));
    
    float* output_data = malloc(buffer_size);
    float scalar = 5.0f;
    
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    
    for (int op = 0; op < OP_COUNT; op++) {
        printf("\n--- %s (scalar=%.1f) ---\n", op_names[op], scalar);
        
        VK_CHECK(vkResetCommandBuffer(cmd, 0));
        VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
        
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[op]);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                                0, 1, &desc_set, 0, NULL);
        vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                          0, sizeof(float), &scalar);
        vkCmdDispatch(cmd, vkc_div_ceil(ARRAY_SIZE, 256), 1, 1);
        
        VK_CHECK(vkEndCommandBuffer(cmd));
        VK_CHECK(vkc_submit_and_wait(&ctx, cmd));
        
        VK_CHECK(vkc_download_buffer(&ctx, &output_buf, output_data, buffer_size));
        
        printf("First 5 results:\n");
        for (int i = 0; i < 5; i++) {
            printf("  [%d] %.1f -> %.1f\n", i, input_data[i], output_data[i]);
        }
    }
    
    // ========================================================================
    // Demonstrate pipeline chaining in single command buffer
    // ========================================================================
    
    printf("\n=== Pipeline Chaining (Add then Multiply) ===\n");
    
    // Create second output buffer for chaining
    VkcBuffer chain_buf;
    VK_CHECK(vkc_create_buffer(&ctx, buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &chain_buf));
    
    // Update descriptor to point output to chain buffer for second operation
    VkDescriptorBufferInfo chain_infos[] = {
        { .buffer = output_buf.buffer, .offset = 0, .range = VK_WHOLE_SIZE },
        { .buffer = chain_buf.buffer, .offset = 0, .range = VK_WHOLE_SIZE }
    };
    
    VkDescriptorSet chain_set;
    VkDescriptorSetAllocateInfo chain_alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = desc_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &desc_layout
    };
    
    // Need another descriptor set for chaining
    VkDescriptorPoolCreateInfo pool2_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size
    };
    VkDescriptorPool desc_pool2;
    VK_CHECK(vkCreateDescriptorPool(ctx.device, &pool2_info, NULL, &desc_pool2));
    chain_alloc.descriptorPool = desc_pool2;
    VK_CHECK(vkAllocateDescriptorSets(ctx.device, &chain_alloc, &chain_set));
    
    VkWriteDescriptorSet chain_writes[] = {
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = chain_set,
          .dstBinding = 0, .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &chain_infos[0] },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = chain_set,
          .dstBinding = 1, .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &chain_infos[1] }
    };
    vkUpdateDescriptorSets(ctx.device, 2, chain_writes, 0, NULL);
    
    // Record chained operations
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
    
    // First: Add
    float add_val = 10.0f;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[OP_ADD]);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                            0, 1, &desc_set, 0, NULL);
    vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float), &add_val);
    vkCmdDispatch(cmd, vkc_div_ceil(ARRAY_SIZE, 256), 1, 1);
    
    // Memory barrier between operations
    VkMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 1, &barrier, 0, NULL, 0, NULL);
    
    // Second: Multiply (using output of add as input)
    float mul_val = 2.0f;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[OP_MULTIPLY]);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                            0, 1, &chain_set, 0, NULL);
    vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float), &mul_val);
    vkCmdDispatch(cmd, vkc_div_ceil(ARRAY_SIZE, 256), 1, 1);
    
    VK_CHECK(vkEndCommandBuffer(cmd));
    VK_CHECK(vkc_submit_and_wait(&ctx, cmd));
    
    float* chain_data = malloc(buffer_size);
    VK_CHECK(vkc_download_buffer(&ctx, &chain_buf, chain_data, buffer_size));
    
    printf("Chain: (input + %.1f) * %.1f\n", add_val, mul_val);
    printf("First 5 results:\n");
    for (int i = 0; i < 5; i++) {
        float expected = (input_data[i] + add_val) * mul_val;
        printf("  [%d] (%.1f + %.1f) * %.1f = %.1f (got %.1f) %s\n", 
               i, input_data[i], add_val, mul_val, expected, chain_data[i],
               chain_data[i] == expected ? "✓" : "✗");
    }
    
    // Cleanup
    free(input_data);
    free(output_data);
    free(chain_data);
    
    for (int i = 0; i < OP_COUNT; i++) {
        vkDestroyPipeline(ctx.device, pipelines[i], NULL);
        vkDestroyShaderModule(ctx.device, shaders[i], NULL);
    }
    
    vkDestroyPipelineCache(ctx.device, pipeline_cache, NULL);
    vkDestroyPipelineLayout(ctx.device, pipeline_layout, NULL);
    vkDestroyDescriptorPool(ctx.device, desc_pool, NULL);
    vkDestroyDescriptorPool(ctx.device, desc_pool2, NULL);
    vkDestroyDescriptorSetLayout(ctx.device, desc_layout, NULL);
    vkDestroyCommandPool(ctx.device, cmd_pool, NULL);
    vkc_destroy_buffer(&ctx, &input_buf);
    vkc_destroy_buffer(&ctx, &output_buf);
    vkc_destroy_buffer(&ctx, &chain_buf);
    vkc_cleanup(&ctx);
    
    printf("\nEpisode 04 completed!\n");
    return 0;
}

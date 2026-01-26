/**
 * VKCompute - Chapter 03: Descriptor Sets Deep Dive
 * 
 * Master descriptor sets, layouts, and pools for binding resources to shaders.
 * 
 * Topics covered:
 * - Descriptor types (storage buffers, uniform buffers, images)
 * - Descriptor set layouts and compatibility
 * - Descriptor pools and allocation strategies
 * - Multiple descriptor sets
 * - Dynamic descriptors
 * - Updating descriptors efficiently
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vk_init.h"
#include "vk_utils.h"
#include "file_utils.h"

#define ARRAY_SIZE 1024

// Parameters passed via uniform buffer
typedef struct {
    float scale;
    float offset;
    uint32_t count;
    uint32_t _padding;
} ComputeParams;

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    
    printf("==============================================\n");
    printf("  VKCompute - Chapter 03: Descriptor Sets\n");
    printf("==============================================\n\n");
    
    VkcContext ctx;
    VK_CHECK(vkc_init(&ctx, NULL));
    
    VkCommandPool cmd_pool;
    VK_CHECK(vkc_create_command_pool(&ctx, &cmd_pool));
    
    VkDeviceSize data_size = ARRAY_SIZE * sizeof(float);
    
    // ========================================================================
    // Create buffers with different descriptor types
    // ========================================================================
    
    printf("Creating buffers...\n");
    
    // Input buffer (storage buffer - read/write)
    VkcBuffer input_buffer;
    VK_CHECK(vkc_create_buffer(&ctx, data_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &input_buffer));
    
    // Output buffer (storage buffer)
    VkcBuffer output_buffer;
    VK_CHECK(vkc_create_buffer(&ctx, data_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &output_buffer));
    
    // Uniform buffer (for parameters - small, frequently updated)
    VkcBuffer uniform_buffer;
    VK_CHECK(vkc_create_buffer(&ctx, sizeof(ComputeParams),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &uniform_buffer));
    
    // Fill input data
    float* input_data = malloc(data_size);
    for (int i = 0; i < ARRAY_SIZE; i++) {
        input_data[i] = (float)i;
    }
    VK_CHECK(vkc_upload_buffer(&ctx, &input_buffer, input_data, data_size));
    
    // Set parameters
    ComputeParams params = {
        .scale = 2.0f,
        .offset = 10.0f,
        .count = ARRAY_SIZE
    };
    VK_CHECK(vkc_upload_buffer(&ctx, &uniform_buffer, &params, sizeof(params)));
    
    printf("  Input: [0, 1, 2, ...]\n");
    printf("  Params: scale=%.1f, offset=%.1f\n", params.scale, params.offset);
    
    // ========================================================================
    // Create descriptor set layouts
    // ========================================================================
    
    printf("\nCreating descriptor layouts...\n");
    
    // Set 0: Uniform buffer (parameters)
    VkDescriptorSetLayoutBinding set0_bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
        }
    };
    
    VkDescriptorSetLayoutCreateInfo set0_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = set0_bindings
    };
    
    VkDescriptorSetLayout set0_layout;
    VK_CHECK(vkCreateDescriptorSetLayout(ctx.device, &set0_layout_info, NULL, &set0_layout));
    printf("  Set 0: Uniform buffer (parameters)\n");
    
    // Set 1: Storage buffers (data)
    VkDescriptorSetLayoutBinding set1_bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
        }
    };
    
    VkDescriptorSetLayoutCreateInfo set1_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = set1_bindings
    };
    
    VkDescriptorSetLayout set1_layout;
    VK_CHECK(vkCreateDescriptorSetLayout(ctx.device, &set1_layout_info, NULL, &set1_layout));
    printf("  Set 1: Storage buffers (input, output)\n");
    
    // ========================================================================
    // Create pipeline layout with multiple sets
    // ========================================================================
    
    VkDescriptorSetLayout all_layouts[] = { set0_layout, set1_layout };
    
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 2,
        .pSetLayouts = all_layouts
    };
    
    VkPipelineLayout pipeline_layout;
    VK_CHECK(vkCreatePipelineLayout(ctx.device, &pipeline_layout_info, NULL, &pipeline_layout));
    
    // ========================================================================
    // Create descriptor pool
    // ========================================================================
    
    printf("\nCreating descriptor pool...\n");
    
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 }
    };
    
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 2,  // We'll allocate 2 descriptor sets
        .poolSizeCount = 2,
        .pPoolSizes = pool_sizes
    };
    
    VkDescriptorPool desc_pool;
    VK_CHECK(vkCreateDescriptorPool(ctx.device, &pool_info, NULL, &desc_pool));
    
    // ========================================================================
    // Allocate descriptor sets
    // ========================================================================
    
    printf("Allocating descriptor sets...\n");
    
    VkDescriptorSetAllocateInfo set_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = desc_pool,
        .descriptorSetCount = 2,
        .pSetLayouts = all_layouts
    };
    
    VkDescriptorSet desc_sets[2];
    VK_CHECK(vkAllocateDescriptorSets(ctx.device, &set_alloc_info, desc_sets));
    
    // ========================================================================
    // Update descriptor sets with buffer info
    // ========================================================================
    
    printf("Updating descriptor sets...\n");
    
    // Update set 0 (uniform buffer)
    VkDescriptorBufferInfo uniform_info = {
        .buffer = uniform_buffer.buffer,
        .offset = 0,
        .range = sizeof(ComputeParams)
    };
    
    // Update set 1 (storage buffers)
    VkDescriptorBufferInfo storage_infos[] = {
        { .buffer = input_buffer.buffer, .offset = 0, .range = VK_WHOLE_SIZE },
        { .buffer = output_buffer.buffer, .offset = 0, .range = VK_WHOLE_SIZE }
    };
    
    VkWriteDescriptorSet writes[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = desc_sets[0],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &uniform_info
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = desc_sets[1],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &storage_infos[0]
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = desc_sets[1],
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &storage_infos[1]
        }
    };
    
    vkUpdateDescriptorSets(ctx.device, 3, writes, 0, NULL);
    
    // ========================================================================
    // Load shader and create pipeline
    // ========================================================================
    
    char* shader_path = vkc_path_relative_to_exe("shaders/ch03_descriptors/transform.comp.spv");
    VkShaderModule shader;
    VK_CHECK(vkc_load_shader(&ctx, shader_path, &shader));
    free(shader_path);
    
    VkPipeline pipeline;
    VK_CHECK(vkc_create_compute_pipeline(&ctx, shader, pipeline_layout, &pipeline));
    
    // ========================================================================
    // Record and execute
    // ========================================================================
    
    printf("\nExecuting compute shader...\n");
    
    VkCommandBuffer cmd;
    VK_CHECK(vkc_create_command_buffer(&ctx, cmd_pool, &cmd));
    
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
    
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    
    // Bind both descriptor sets at once
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                            0, 2, desc_sets, 0, NULL);
    
    vkCmdDispatch(cmd, vkc_div_ceil(ARRAY_SIZE, 256), 1, 1);
    
    VK_CHECK(vkEndCommandBuffer(cmd));
    VK_CHECK(vkc_submit_and_wait(&ctx, cmd));
    
    // ========================================================================
    // Read and verify results
    // ========================================================================
    
    float* output_data = malloc(data_size);
    VK_CHECK(vkc_download_buffer(&ctx, &output_buffer, output_data, data_size));
    
    printf("\n=== Results ===\n");
    printf("Formula: output = input * scale + offset\n");
    printf("First 5 values:\n");
    for (int i = 0; i < 5; i++) {
        float expected = input_data[i] * params.scale + params.offset;
        printf("  %d * %.1f + %.1f = %.1f (got %.1f) %s\n", 
               i, params.scale, params.offset, expected, output_data[i],
               (output_data[i] == expected) ? "✓" : "✗");
    }
    
    bool success = true;
    for (int i = 0; i < ARRAY_SIZE; i++) {
        float expected = input_data[i] * params.scale + params.offset;
        if (output_data[i] != expected) {
            success = false;
            break;
        }
    }
    
    if (success) {
        printf("\n✓ All %d values correct!\n", ARRAY_SIZE);
    }
    
    // ========================================================================
    // Demonstrate updating parameters and re-running
    // ========================================================================
    
    printf("\n--- Updating parameters and re-running ---\n");
    
    params.scale = 0.5f;
    params.offset = -100.0f;
    VK_CHECK(vkc_upload_buffer(&ctx, &uniform_buffer, &params, sizeof(params)));
    printf("New params: scale=%.1f, offset=%.1f\n", params.scale, params.offset);
    
    // Re-run with same command buffer setup
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                            0, 2, desc_sets, 0, NULL);
    vkCmdDispatch(cmd, vkc_div_ceil(ARRAY_SIZE, 256), 1, 1);
    VK_CHECK(vkEndCommandBuffer(cmd));
    VK_CHECK(vkc_submit_and_wait(&ctx, cmd));
    
    VK_CHECK(vkc_download_buffer(&ctx, &output_buffer, output_data, data_size));
    
    printf("First 5 values with new params:\n");
    for (int i = 0; i < 5; i++) {
        float expected = input_data[i] * params.scale + params.offset;
        printf("  %d * %.1f + %.1f = %.1f (got %.1f) %s\n", 
               i, params.scale, params.offset, expected, output_data[i],
               (output_data[i] == expected) ? "✓" : "✗");
    }
    
    // Cleanup
    free(input_data);
    free(output_data);
    vkDestroyPipeline(ctx.device, pipeline, NULL);
    vkDestroyPipelineLayout(ctx.device, pipeline_layout, NULL);
    vkDestroyDescriptorPool(ctx.device, desc_pool, NULL);
    vkDestroyDescriptorSetLayout(ctx.device, set0_layout, NULL);
    vkDestroyDescriptorSetLayout(ctx.device, set1_layout, NULL);
    vkDestroyShaderModule(ctx.device, shader, NULL);
    vkDestroyCommandPool(ctx.device, cmd_pool, NULL);
    vkc_destroy_buffer(&ctx, &input_buffer);
    vkc_destroy_buffer(&ctx, &output_buffer);
    vkc_destroy_buffer(&ctx, &uniform_buffer);
    vkc_cleanup(&ctx);
    
    printf("\nChapter 03 completed!\n");
    return 0;
}

/**
 * VKCompute - Chapter 08: Specialization Constants
 * 
 * Use specialization constants for compile-time shader optimization.
 * 
 * Topics covered:
 * - Specialization constant basics
 * - Setting constants at pipeline creation time
 * - Workgroup size specialization
 * - Algorithm selection via specialization
 * - Performance benefits of compile-time constants
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "vk_init.h"
#include "vk_utils.h"
#include "file_utils.h"

#define ARRAY_SIZE (256 * 1024)  // 256K elements


// Specialization constant IDs (must match shader)
#define SPEC_WORKGROUP_SIZE_ID  0
#define SPEC_OPERATION_ID       1
#define SPEC_UNROLL_FACTOR_ID   2

// Operation types
#define OP_ADD      0
#define OP_MULTIPLY 1
#define OP_FMA      2  // Fused multiply-add

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    
    printf("==============================================\n");
    printf("  VKCompute - Chapter 08: Specialization\n");
    printf("==============================================\n\n");
    
    VkcContext ctx;
    VK_CHECK(vkc_init(&ctx, NULL));
    
    // ========================================================================
    // Check device limits for workgroup size
    // ========================================================================
    
    printf("=== Device Limits ===\n");
    printf("  Max Workgroup Size X: %u\n", ctx.device_props.limits.maxComputeWorkGroupSize[0]);
    printf("  Max Workgroup Invocations: %u\n", ctx.device_props.limits.maxComputeWorkGroupInvocations);
    
    VkCommandPool cmd_pool;
    VK_CHECK(vkc_create_command_pool(&ctx, &cmd_pool));
    
    // Create buffers
    VkDeviceSize buffer_size = ARRAY_SIZE * sizeof(float);
    
    VkcBuffer buf_a, buf_b, buf_c;
    VK_CHECK(vkc_create_buffer(&ctx, buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &buf_a));
    VK_CHECK(vkc_create_buffer(&ctx, buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &buf_b));
    VK_CHECK(vkc_create_buffer(&ctx, buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &buf_c));
    
    // Initialize data
    float* data_a = malloc(buffer_size);
    float* data_b = malloc(buffer_size);
    for (int i = 0; i < ARRAY_SIZE; i++) {
        data_a[i] = (float)i;
        data_b[i] = (float)(ARRAY_SIZE - i);
    }
    VK_CHECK(vkc_upload_buffer(&ctx, &buf_a, data_a, buffer_size));
    VK_CHECK(vkc_upload_buffer(&ctx, &buf_b, data_b, buffer_size));
    
    // ========================================================================
    // Load shader module
    // ========================================================================
    
    char* shader_path = vkc_path_relative_to_exe("shaders/ch08_specialization/compute.comp.spv");
    VkShaderModule shader;
    VK_CHECK(vkc_load_shader(&ctx, shader_path, &shader));
    free(shader_path);
    
    // ========================================================================
    // Create descriptor set layout
    // ========================================================================
    
    VkDescriptorSetLayoutBinding bindings[] = {
        { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
        { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
        { .binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT }
    };
    
    VkDescriptorSetLayoutCreateInfo desc_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 3, .pBindings = bindings
    };
    
    VkDescriptorSetLayout desc_layout;
    VK_CHECK(vkCreateDescriptorSetLayout(ctx.device, &desc_layout_info, NULL, &desc_layout));
    
    // Push constant for scale factor
    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = sizeof(float)
    };
    
    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1, .pSetLayouts = &desc_layout,
        .pushConstantRangeCount = 1, .pPushConstantRanges = &push_range
    };
    
    VkPipelineLayout pipeline_layout;
    VK_CHECK(vkCreatePipelineLayout(ctx.device, &layout_info, NULL, &pipeline_layout));
    
    // Descriptor pool and set
    VkDescriptorPoolSize pool_size = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 };
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
        { buf_a.buffer, 0, VK_WHOLE_SIZE },
        { buf_b.buffer, 0, VK_WHOLE_SIZE },
        { buf_c.buffer, 0, VK_WHOLE_SIZE }
    };
    
    VkWriteDescriptorSet writes[] = {
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = desc_set,
          .dstBinding = 0, .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buf_infos[0] },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = desc_set,
          .dstBinding = 1, .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buf_infos[1] },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = desc_set,
          .dstBinding = 2, .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buf_infos[2] }
    };
    vkUpdateDescriptorSets(ctx.device, 3, writes, 0, NULL);
    
    // ========================================================================
    // Create pipelines with different specialization constants
    // ========================================================================
    
    printf("\n=== Creating Specialized Pipelines ===\n");
    
    struct {
        const char* name;
        uint32_t workgroup_size;
        uint32_t operation;
        uint32_t unroll_factor;
    } configs[] = {
        { "Add, WG=64",        64,  OP_ADD,      1 },
        { "Add, WG=256",       256, OP_ADD,      1 },
        { "Add, WG=512",       512, OP_ADD,      1 },
        { "Multiply, WG=256",  256, OP_MULTIPLY, 1 },
        { "FMA, WG=256",       256, OP_FMA,      1 },
        { "FMA, WG=256, UF=4", 256, OP_FMA,      4 }
    };
    
    int num_configs = sizeof(configs) / sizeof(configs[0]);
    VkPipeline* pipelines = malloc(sizeof(VkPipeline) * num_configs);
    
    for (int i = 0; i < num_configs; i++) {
        // Skip workgroup sizes larger than device supports
        if (configs[i].workgroup_size > ctx.device_props.limits.maxComputeWorkGroupSize[0]) {
            printf("  Skipping %s (workgroup too large)\n", configs[i].name);
            pipelines[i] = VK_NULL_HANDLE;
            continue;
        }
        
        // Define specialization map entries
        VkSpecializationMapEntry map_entries[] = {
            { .constantID = SPEC_WORKGROUP_SIZE_ID, .offset = 0, .size = sizeof(uint32_t) },
            { .constantID = SPEC_OPERATION_ID,      .offset = 4, .size = sizeof(uint32_t) },
            { .constantID = SPEC_UNROLL_FACTOR_ID,  .offset = 8, .size = sizeof(uint32_t) }
        };
        
        // Pack constant data
        uint32_t spec_data[3] = {
            configs[i].workgroup_size,
            configs[i].operation,
            configs[i].unroll_factor
        };
        
        VkSpecializationInfo spec_info = {
            .mapEntryCount = 3,
            .pMapEntries = map_entries,
            .dataSize = sizeof(spec_data),
            .pData = spec_data
        };
        
        VkPipelineShaderStageCreateInfo stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader,
            .pName = "main",
            .pSpecializationInfo = &spec_info  // Key: attach specialization info!
        };
        
        VkComputePipelineCreateInfo pipeline_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = stage,
            .layout = pipeline_layout
        };
        
        double start = vkc_get_time_ms();
        VK_CHECK(vkCreateComputePipelines(ctx.device, VK_NULL_HANDLE, 1, 
                                          &pipeline_info, NULL, &pipelines[i]));
        double elapsed = vkc_get_time_ms() - start;
        
        printf("  Created: %s (%.2f ms)\n", configs[i].name, elapsed);
    }
    
    // ========================================================================
    // Run and benchmark each configuration
    // ========================================================================
    
    printf("\n=== Benchmarking Specialized Pipelines ===\n");
    
    VkCommandBuffer cmd;
    VK_CHECK(vkc_create_command_buffer(&ctx, cmd_pool, &cmd));
    
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    
    float* output_result = malloc(buffer_size);
    float scale = 2.0f;
    
    for (int i = 0; i < num_configs; i++) {
        if (pipelines[i] == VK_NULL_HANDLE) continue;
        
        uint32_t workgroup_count = vkc_div_ceil(ARRAY_SIZE, configs[i].workgroup_size);
        
        // Warmup
        VK_CHECK(vkResetCommandBuffer(cmd, 0));
        VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[i]);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                                0, 1, &desc_set, 0, NULL);
        vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float), &scale);
        vkCmdDispatch(cmd, workgroup_count, 1, 1);
        VK_CHECK(vkEndCommandBuffer(cmd));
        VK_CHECK(vkc_submit_and_wait(&ctx, cmd));
        
        // Benchmark
        int iterations = 100;
        double start = vkc_get_time_ms();
        
        for (int j = 0; j < iterations; j++) {
            VK_CHECK(vkResetCommandBuffer(cmd, 0));
            VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[i]);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                                    0, 1, &desc_set, 0, NULL);
            vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float), &scale);
            vkCmdDispatch(cmd, workgroup_count, 1, 1);
            VK_CHECK(vkEndCommandBuffer(cmd));
            VK_CHECK(vkc_submit_and_wait(&ctx, cmd));
        }
        
        double elapsed = vkc_get_time_ms() - start;
        double per_iter = elapsed / iterations;
        double throughput = (ARRAY_SIZE * sizeof(float) * 3) / (per_iter / 1000.0) / (1024.0 * 1024.0 * 1024.0);
        
        printf("  %s:\n", configs[i].name);
        printf("    Time: %.3f ms/iter, Throughput: %.2f GB/s\n", per_iter, throughput);
        
        // Verify one result
        vkc_download_buffer(&ctx, &buf_c, output_result, buffer_size);
        float expected;
        switch (configs[i].operation) {
            case OP_ADD:      expected = data_a[0] + data_b[0]; break;
            case OP_MULTIPLY: expected = data_a[0] * data_b[0]; break;
            case OP_FMA:      expected = data_a[0] * scale + data_b[0]; break;
            default:          expected = 0; break;
        }
        printf("    Verify: result[0] = %.1f (expected %.1f) %s\n", 
               output_result[0], expected, output_result[0] == expected ? "✓" : "✗");
    }
    
    // ========================================================================
    // Demonstrate operation differences
    // ========================================================================
    
    printf("\n=== Operation Results Comparison ===\n");
    printf("Input: a[5] = %.1f, b[5] = %.1f, scale = %.1f\n", data_a[5], data_b[5], scale);
    
    for (int i = 0; i < num_configs; i++) {
        if (pipelines[i] == VK_NULL_HANDLE) continue;
        
        VK_CHECK(vkResetCommandBuffer(cmd, 0));
        VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines[i]);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
                                0, 1, &desc_set, 0, NULL);
        vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float), &scale);
        vkCmdDispatch(cmd, vkc_div_ceil(ARRAY_SIZE, configs[i].workgroup_size), 1, 1);
        VK_CHECK(vkEndCommandBuffer(cmd));
        VK_CHECK(vkc_submit_and_wait(&ctx, cmd));
        
        vkc_download_buffer(&ctx, &buf_c, output_result, buffer_size);
        printf("  %s: result[5] = %.1f\n", configs[i].name, output_result[5]);
    }
    
    // Cleanup
    free(data_a);
    free(data_b);
    free(output_result);
    
    for (int i = 0; i < num_configs; i++) {
        if (pipelines[i] != VK_NULL_HANDLE) {
            vkDestroyPipeline(ctx.device, pipelines[i], NULL);
        }
    }
    free(pipelines);
    
    vkDestroyPipelineLayout(ctx.device, pipeline_layout, NULL);
    vkDestroyDescriptorPool(ctx.device, desc_pool, NULL);
    vkDestroyDescriptorSetLayout(ctx.device, desc_layout, NULL);
    vkDestroyShaderModule(ctx.device, shader, NULL);
    vkDestroyCommandPool(ctx.device, cmd_pool, NULL);
    vkc_destroy_buffer(&ctx, &buf_a);
    vkc_destroy_buffer(&ctx, &buf_b);
    vkc_destroy_buffer(&ctx, &buf_c);
    vkc_cleanup(&ctx);
    
    printf("\nChapter 08 completed!\n");
    return 0;
}

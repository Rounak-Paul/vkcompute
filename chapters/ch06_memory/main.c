/**
 * VKCompute - Chapter 06: Memory Management
 * 
 * Deep dive into Vulkan memory types and allocation strategies.
 * 
 * Topics covered:
 * - Memory heaps and types explained
 * - Choosing the right memory type
 * - Memory alignment requirements
 * - Sub-allocation from large blocks
 * - Memory mapping strategies
 * - Flushing and invalidating non-coherent memory
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "vk_init.h"
#include "vk_utils.h"
#include "file_utils.h"

#define ARRAY_SIZE 1024
#define LARGE_BLOCK_SIZE (16 * 1024 * 1024)  // 16 MB


// Print detailed memory properties
static void print_memory_details(VkPhysicalDevice device) {
    VkPhysicalDeviceMemoryProperties mem;
    vkGetPhysicalDeviceMemoryProperties(device, &mem);
    
    printf("=== Memory Heaps (%u) ===\n", mem.memoryHeapCount);
    for (uint32_t i = 0; i < mem.memoryHeapCount; i++) {
        printf("  Heap %u:\n", i);
        printf("    Size: %.2f GB\n", mem.memoryHeaps[i].size / (1024.0 * 1024.0 * 1024.0));
        printf("    Flags: ");
        if (mem.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) 
            printf("DEVICE_LOCAL ");
        if (mem.memoryHeaps[i].flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT)
            printf("MULTI_INSTANCE ");
        if (mem.memoryHeaps[i].flags == 0)
            printf("(system memory)");
        printf("\n");
    }
    
    printf("\n=== Memory Types (%u) ===\n", mem.memoryTypeCount);
    for (uint32_t i = 0; i < mem.memoryTypeCount; i++) {
        VkMemoryPropertyFlags flags = mem.memoryTypes[i].propertyFlags;
        printf("  Type %u (Heap %u):\n", i, mem.memoryTypes[i].heapIndex);
        printf("    Properties: ");
        if (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) printf("DEVICE_LOCAL ");
        if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) printf("HOST_VISIBLE ");
        if (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) printf("HOST_COHERENT ");
        if (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) printf("HOST_CACHED ");
        if (flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) printf("LAZILY_ALLOCATED ");
        if (flags == 0) printf("(none)");
        printf("\n");
        
        // Explain what this type is good for
        printf("    Best for: ");
        if ((flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) && 
            !(flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            printf("GPU-only data (fastest compute)");
        } else if ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) && 
                   (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            printf("Staging buffers, uniform buffers");
        } else if ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) && 
                   (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)) {
            printf("Readback buffers");
        } else if ((flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) && 
                   (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            printf("ReBAR/SAM - CPU-visible VRAM (ideal!)");
        }
        printf("\n");
    }
}

// Find memory type with specific properties
static int32_t find_memory_type_with_properties(
    VkPhysicalDevice device, 
    uint32_t type_filter,
    VkMemoryPropertyFlags required,
    VkMemoryPropertyFlags preferred) 
{
    VkPhysicalDeviceMemoryProperties mem;
    vkGetPhysicalDeviceMemoryProperties(device, &mem);
    
    // First try to find with preferred properties
    for (uint32_t i = 0; i < mem.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (mem.memoryTypes[i].propertyFlags & (required | preferred)) == (required | preferred)) {
            return (int32_t)i;
        }
    }
    
    // Fall back to just required
    for (uint32_t i = 0; i < mem.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (mem.memoryTypes[i].propertyFlags & required) == required) {
            return (int32_t)i;
        }
    }
    
    return -1;
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    
    printf("==============================================\n");
    printf("  VKCompute - Chapter 06: Memory Management\n");
    printf("==============================================\n\n");
    
    VkcContext ctx;
    VK_CHECK(vkc_init(&ctx, NULL));
    
    // ========================================================================
    // Part 1: Explore memory types
    // ========================================================================
    
    print_memory_details(ctx.physical_device);
    
    // Print device limits
    printf("\n=== Memory Limits ===\n");
    printf("  Max Memory Allocations: %u\n", ctx.device_props.limits.maxMemoryAllocationCount);
    printf("  Buffer-Image Granularity: %lu bytes\n", 
           (unsigned long)ctx.device_props.limits.bufferImageGranularity);
    printf("  Non-Coherent Atom Size: %lu bytes\n",
           (unsigned long)ctx.device_props.limits.nonCoherentAtomSize);
    
    // ========================================================================
    // Part 2: Demonstrate alignment requirements
    // ========================================================================
    
    printf("\n=== Part 2: Alignment Requirements ===\n");
    
    // Create test buffers to check alignment
    VkBufferCreateInfo buf_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = 1024,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    
    VkBuffer test_buf;
    VK_CHECK(vkCreateBuffer(ctx.device, &buf_info, NULL, &test_buf));
    
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(ctx.device, test_buf, &mem_req);
    
    printf("Storage Buffer (1024 bytes):\n");
    printf("  Actual size needed: %lu bytes\n", (unsigned long)mem_req.size);
    printf("  Alignment required: %lu bytes\n", (unsigned long)mem_req.alignment);
    printf("  Memory type bits: 0x%X\n", mem_req.memoryTypeBits);
    
    vkDestroyBuffer(ctx.device, test_buf, NULL);
    
    // Uniform buffer alignment
    buf_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    VK_CHECK(vkCreateBuffer(ctx.device, &buf_info, NULL, &test_buf));
    vkGetBufferMemoryRequirements(ctx.device, test_buf, &mem_req);
    
    printf("\nUniform Buffer (1024 bytes):\n");
    printf("  Alignment required: %lu bytes\n", (unsigned long)mem_req.alignment);
    printf("  Min Uniform Buffer Offset Alignment: %lu bytes\n",
           (unsigned long)ctx.device_props.limits.minUniformBufferOffsetAlignment);
    
    vkDestroyBuffer(ctx.device, test_buf, NULL);
    
    // ========================================================================
    // Part 3: Sub-allocation from a large memory block
    // ========================================================================
    
    printf("\n=== Part 3: Sub-allocation ===\n");
    printf("Allocating %.2f MB block for multiple buffers...\n", 
           LARGE_BLOCK_SIZE / (1024.0 * 1024.0));
    
    // Allocate a large device memory block
    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = LARGE_BLOCK_SIZE,
        .memoryTypeIndex = (uint32_t)find_memory_type_with_properties(
            ctx.physical_device, 0xFFFFFFFF,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    
    VkDeviceMemory large_block;
    VK_CHECK(vkAllocateMemory(ctx.device, &alloc_info, NULL, &large_block));
    
    // Create multiple buffers that share this memory
    #define NUM_SUBALLOC_BUFFERS 4
    VkBuffer sub_buffers[NUM_SUBALLOC_BUFFERS];
    VkDeviceSize sub_sizes[NUM_SUBALLOC_BUFFERS] = { 1024, 4096, 2048, 8192 };
    VkDeviceSize offsets[NUM_SUBALLOC_BUFFERS];
    VkDeviceSize current_offset = 0;
    
    for (int i = 0; i < NUM_SUBALLOC_BUFFERS; i++) {
        buf_info.size = sub_sizes[i];
        buf_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        VK_CHECK(vkCreateBuffer(ctx.device, &buf_info, NULL, &sub_buffers[i]));
        
        vkGetBufferMemoryRequirements(ctx.device, sub_buffers[i], &mem_req);
        
        // Align offset
        current_offset = vkc_align_up((uint32_t)current_offset, (uint32_t)mem_req.alignment);
        offsets[i] = current_offset;
        
        VK_CHECK(vkBindBufferMemory(ctx.device, sub_buffers[i], large_block, offsets[i]));
        
        printf("  Buffer %d: size=%lu, offset=%lu, aligned to %lu\n",
               i, (unsigned long)sub_sizes[i], (unsigned long)offsets[i], 
               (unsigned long)mem_req.alignment);
        
        current_offset += mem_req.size;
    }
    
    printf("Total used: %lu / %d bytes\n", (unsigned long)current_offset, LARGE_BLOCK_SIZE);
    
    // Map once, use for all buffers
    void* mapped_ptr;
    VK_CHECK(vkMapMemory(ctx.device, large_block, 0, VK_WHOLE_SIZE, 0, &mapped_ptr));
    
    // Write to each sub-buffer
    for (int i = 0; i < NUM_SUBALLOC_BUFFERS; i++) {
        float* buf_ptr = (float*)((char*)mapped_ptr + offsets[i]);
        for (size_t j = 0; j < sub_sizes[i] / sizeof(float); j++) {
            buf_ptr[j] = (float)(i * 1000 + j);
        }
    }
    
    printf("Wrote test data to all sub-buffers\n");
    
    // ========================================================================
    // Part 4: Run compute on sub-allocated buffer
    // ========================================================================
    
    printf("\n=== Part 4: Compute on Sub-allocated Buffer ===\n");
    
    VkCommandPool cmd_pool;
    VK_CHECK(vkc_create_command_pool(&ctx, &cmd_pool));
    
    char* shader_path = vkc_path_relative_to_exe("shaders/ch06_memory/double.comp.spv");
    VkShaderModule shader;
    VK_CHECK(vkc_load_shader(&ctx, shader_path, &shader));
    free(shader_path);
    
    VkDescriptorSetLayoutBinding binding = {
        .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
    };
    
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1, .pBindings = &binding
    };
    
    VkDescriptorSetLayout desc_layout;
    VK_CHECK(vkCreateDescriptorSetLayout(ctx.device, &layout_info, NULL, &desc_layout));
    
    VkPipelineLayoutCreateInfo pipe_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1, .pSetLayouts = &desc_layout
    };
    
    VkPipelineLayout pipe_layout;
    VK_CHECK(vkCreatePipelineLayout(ctx.device, &pipe_layout_info, NULL, &pipe_layout));
    
    VkPipeline pipeline;
    VK_CHECK(vkc_create_compute_pipeline(&ctx, shader, pipe_layout, &pipeline));
    
    // Descriptor pool and set for one of the sub-buffers
    VkDescriptorPoolSize pool_size = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &pool_size
    };
    
    VkDescriptorPool desc_pool;
    VK_CHECK(vkCreateDescriptorPool(ctx.device, &pool_info, NULL, &desc_pool));
    
    VkDescriptorSetAllocateInfo desc_alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = desc_pool, .descriptorSetCount = 1, .pSetLayouts = &desc_layout
    };
    
    VkDescriptorSet desc_set;
    VK_CHECK(vkAllocateDescriptorSets(ctx.device, &desc_alloc, &desc_set));
    
    // Use buffer 1 (4096 bytes = 1024 floats)
    VkDescriptorBufferInfo buf_desc = {
        .buffer = sub_buffers[1],
        .offset = 0,
        .range = sub_sizes[1]
    };
    
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = desc_set, .dstBinding = 0, .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buf_desc
    };
    vkUpdateDescriptorSets(ctx.device, 1, &write, 0, NULL);
    
    // Execute
    VkCommandBuffer cmd;
    VK_CHECK(vkc_create_command_buffer(&ctx, cmd_pool, &cmd));
    
    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin));
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe_layout,
                            0, 1, &desc_set, 0, NULL);
    vkCmdDispatch(cmd, (uint32_t)(sub_sizes[1] / sizeof(float) / 256), 1, 1);
    VK_CHECK(vkEndCommandBuffer(cmd));
    
    VK_CHECK(vkc_submit_and_wait(&ctx, cmd));
    
    // Read back (memory is still mapped!)
    float* result = (float*)((char*)mapped_ptr + offsets[1]);
    printf("Buffer 1 after compute (doubled):\n");
    printf("  [0] = %.1f (was 1000.0)\n", result[0]);
    printf("  [1] = %.1f (was 1001.0)\n", result[1]);
    printf("  [2] = %.1f (was 1002.0)\n", result[2]);
    
    // ========================================================================
    // Part 5: Performance comparison
    // ========================================================================
    
    printf("\n=== Part 5: Memory Access Performance ===\n");
    
    VkDeviceSize test_size = 4 * 1024 * 1024;  // 4 MB
    float* test_data = malloc(test_size);
    for (size_t i = 0; i < test_size / sizeof(float); i++) {
        test_data[i] = (float)i;
    }
    
    // Test host-visible coherent
    VkcBuffer coherent_buf;
    VK_CHECK(vkc_create_buffer(&ctx, test_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &coherent_buf));
    
    double start = vkc_get_time_ms();
    for (int i = 0; i < 10; i++) {
        VK_CHECK(vkc_upload_buffer(&ctx, &coherent_buf, test_data, test_size));
    }
    double coherent_time = (vkc_get_time_ms() - start) / 10.0;
    printf("Host-visible coherent: %.2f ms per 4MB upload (%.2f GB/s)\n",
           coherent_time, (test_size / (1024.0 * 1024.0 * 1024.0)) / (coherent_time / 1000.0));
    
    vkc_destroy_buffer(&ctx, &coherent_buf);
    free(test_data);
    
    // Cleanup
    vkUnmapMemory(ctx.device, large_block);
    
    for (int i = 0; i < NUM_SUBALLOC_BUFFERS; i++) {
        vkDestroyBuffer(ctx.device, sub_buffers[i], NULL);
    }
    vkFreeMemory(ctx.device, large_block, NULL);
    
    vkDestroyPipeline(ctx.device, pipeline, NULL);
    vkDestroyPipelineLayout(ctx.device, pipe_layout, NULL);
    vkDestroyDescriptorPool(ctx.device, desc_pool, NULL);
    vkDestroyDescriptorSetLayout(ctx.device, desc_layout, NULL);
    vkDestroyShaderModule(ctx.device, shader, NULL);
    vkDestroyCommandPool(ctx.device, cmd_pool, NULL);
    vkc_cleanup(&ctx);
    
    printf("\nChapter 06 completed!\n");
    return 0;
}

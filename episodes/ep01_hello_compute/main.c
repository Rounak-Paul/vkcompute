/**
 * VKCompute - Episode 01: Hello Vulkan Compute
 * 
 * Your first Vulkan compute shader! We keep it simple:
 * A compute shader that doubles every value in an array.
 * 
 * This episode focuses on the high-level flow:
 * 1. Initialize Vulkan
 * 2. Create buffers for input/output
 * 3. Run a compute shader
 * 4. Read back results
 * 
 * We use helper functions to hide the complexity - 
 * later episodes will dive into the details!
 */

#include <stdio.h>
#include <stdlib.h>

#include "vk_init.h"
#include "vk_utils.h"
#include "file_utils.h"

#define ARRAY_SIZE 1024

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    printf("==============================================\n");
    printf("  VKCompute - Episode 01: Hello Vulkan Compute\n");
    printf("==============================================\n\n");
    
    // ========================================================================
    // Step 1: Initialize Vulkan
    // ========================================================================
    // The vkc_init() helper creates a Vulkan instance, finds a GPU with
    // compute capability, and sets up a logical device with a compute queue.
    
    VkcContext ctx;
    VkcConfig config = vkc_config_default();
    config.app_name = "EP01 Hello Compute";
    
    if (vkc_init(&ctx, &config) != VK_SUCCESS) {
        fprintf(stderr, "Failed to initialize Vulkan!\n");
        return 1;
    }
    
    printf("Vulkan initialized successfully!\n");
    vkc_print_device_info(ctx.physical_device);
    
    // ========================================================================
    // Step 2: Prepare input data
    // ========================================================================
    // We'll create an array of numbers [0, 1, 2, 3, ...] and double each one.
    
    float input_data[ARRAY_SIZE];
    for (int i = 0; i < ARRAY_SIZE; i++) {
        input_data[i] = (float)i;
    }
    printf("\nInput: [0, 1, 2, 3, ... %d]\n", ARRAY_SIZE - 1);
    
    // ========================================================================
    // Step 3: Create GPU buffers
    // ========================================================================
    // Buffers are how we send data to and from the GPU.
    // - Input buffer: holds our source data
    // - Output buffer: where the shader writes results
    
    VkDeviceSize buffer_size = ARRAY_SIZE * sizeof(float);
    
    VkcBuffer input_buffer, output_buffer;
    
    VK_CHECK(vkc_create_buffer(&ctx, buffer_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &input_buffer));
    
    VK_CHECK(vkc_create_buffer(&ctx, buffer_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &output_buffer));
    
    // Upload input data to GPU
    VK_CHECK(vkc_upload_buffer(&ctx, &input_buffer, input_data, buffer_size));
    printf("Created input and output buffers (%zu bytes each)\n", (size_t)buffer_size);
    
    // ========================================================================
    // Step 4: Load and run the compute shader
    // ========================================================================
    // The shader is written in GLSL and compiled to SPIR-V.
    // Our helper function handles all the pipeline setup internally.
    
    char* shader_path = vkc_path_relative_to_exe("shaders/ep01_hello_compute/double.comp.spv");
    printf("Loading shader: %s\n", shader_path);
    
    printf("\nRunning compute shader...\n");
    VK_CHECK(vkc_run_simple_compute(&ctx, shader_path, 
                                     &input_buffer, &output_buffer, 
                                     ARRAY_SIZE));
    printf("Compute shader completed!\n");
    free(shader_path);
    
    // ========================================================================
    // Step 5: Read back and verify results  
    // ========================================================================
    
    float output_data[ARRAY_SIZE];
    VK_CHECK(vkc_download_buffer(&ctx, &output_buffer, output_data, buffer_size));
    
    printf("\n=== Results ===\n");
    printf("First 10 values:\n");
    for (int i = 0; i < 10; i++) {
        printf("  input[%d] = %.0f  ->  output[%d] = %.0f\n", 
               i, input_data[i], i, output_data[i]);
    }
    
    // Verify all values are correct
    int errors = 0;
    for (int i = 0; i < ARRAY_SIZE; i++) {
        float expected = input_data[i] * 2.0f;
        if (output_data[i] != expected) {
            if (errors < 5) {
                printf("ERROR: output[%d] = %.0f, expected %.0f\n", 
                       i, output_data[i], expected);
            }
            errors++;
        }
    }
    
    if (errors == 0) {
        printf("\n✓ SUCCESS! All %d values doubled correctly.\n", ARRAY_SIZE);
    } else {
        printf("\n✗ FAILED: %d errors found\n", errors);
    }
    
    // ========================================================================
    // Cleanup
    // ========================================================================
    
    vkc_destroy_buffer(&ctx, &input_buffer);
    vkc_destroy_buffer(&ctx, &output_buffer);
    vkc_cleanup(&ctx);
    
    printf("\n=== What we learned ===\n");
    printf("1. Initialize Vulkan with vkc_init()\n");
    printf("2. Create buffers to hold GPU data\n");
    printf("3. Run compute shaders on the GPU\n");
    printf("4. Read results back to CPU\n");
    printf("\nNext episode: Dive deeper into buffer types!\n");
    
    return 0;
}

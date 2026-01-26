# Chapter 04: Pipelines

## Overview

A compute pipeline encapsulates the shader and its configuration. This chapter covers:

- Pipeline creation and caching
- Managing multiple pipelines
- Pipeline switching costs
- Chaining compute operations

**What you'll learn:**

- The anatomy of a compute pipeline
- Creating pipeline caches for faster loading
- Efficient multi-pipeline workflows

## What is a Compute Pipeline?

A pipeline combines:

- **Shader module**: The compiled SPIR-V code
- **Pipeline layout**: Descriptor set layouts + push constants
- **Specialization constants**: Compile-time values

```
┌─────────────────────────────────────────┐
│            Compute Pipeline             │
├─────────────────────────────────────────┤
│  Shader Module (SPIR-V)                 │
│  ├── Entry point: "main"                │
│  └── Specialization constants           │
├─────────────────────────────────────────┤
│  Pipeline Layout                        │
│  ├── Descriptor Set Layouts             │
│  └── Push Constant Ranges               │
└─────────────────────────────────────────┘
```

## Creating a Compute Pipeline

### Step 1: Load Shader Module

```c
// Read SPIR-V file
size_t code_size;
uint32_t* code = read_file("shader.comp.spv", &code_size);

VkShaderModuleCreateInfo module_info = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = code_size,
    .pCode = code
};

VkShaderModule shader_module;
vkCreateShaderModule(device, &module_info, NULL, &shader_module);
```

### Step 2: Create Pipeline

```c
VkPipelineShaderStageCreateInfo stage_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_COMPUTE_BIT,
    .module = shader_module,
    .pName = "main"  // Entry point function name
};

VkComputePipelineCreateInfo pipeline_info = {
    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    .stage = stage_info,
    .layout = pipeline_layout
};

VkPipeline pipeline;
vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, 
                         &pipeline_info, NULL, &pipeline);
```

### Cleanup

Shader modules can be destroyed after pipeline creation:

```c
// Shader is compiled into pipeline, module no longer needed
vkDestroyShaderModule(device, shader_module, NULL);
```

## Pipeline Caching

Pipeline creation involves shader compilation — slow! Use a **pipeline cache**:

```c
// Create cache
VkPipelineCacheCreateInfo cache_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
    .initialDataSize = 0,
    .pInitialData = NULL
};

VkPipelineCache cache;
vkCreatePipelineCache(device, &cache_info, NULL, &cache);

// Create pipeline with cache
vkCreateComputePipelines(device, cache, 1, &pipeline_info, 
                         NULL, &pipeline);
```

### Saving and Loading Cache

```c
// Get cache data
size_t cache_size;
vkGetPipelineCacheData(device, cache, &cache_size, NULL);

void* cache_data = malloc(cache_size);
vkGetPipelineCacheData(device, cache, &cache_size, cache_data);

// Save to file
write_file("pipeline.cache", cache_data, cache_size);

// Later: Load cache
void* loaded_data = read_file("pipeline.cache", &cache_size);

VkPipelineCacheCreateInfo cache_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
    .initialDataSize = cache_size,
    .pInitialData = loaded_data
};
```

!!! tip "Cache Invalidation"
    Cache data is driver and GPU specific. It's automatically invalidated when incompatible.

## Multiple Pipelines

Real applications use many pipelines. Create them efficiently:

```c
// Batch creation
VkComputePipelineCreateInfo infos[4] = {
    {/* add shader */},
    {/* multiply shader */},
    {/* square shader */},
    {/* sqrt shader */}
};

VkPipeline pipelines[4];
vkCreateComputePipelines(device, cache, 4, infos, NULL, pipelines);
```

### Pipeline Switching

Switching pipelines has overhead. Minimize switches:

```c
// Bad: Switch pipeline for each element
for (int i = 0; i < N; i++) {
    vkCmdBindPipeline(cmd, ..., pipeline_a);
    vkCmdDispatch(cmd, 1, 1, 1);
    vkCmdBindPipeline(cmd, ..., pipeline_b);
    vkCmdDispatch(cmd, 1, 1, 1);
}

// Good: Batch by pipeline
vkCmdBindPipeline(cmd, ..., pipeline_a);
for (int i = 0; i < N; i++) {
    vkCmdDispatch(cmd, 1, 1, 1);
}
vkCmdBindPipeline(cmd, ..., pipeline_b);
for (int i = 0; i < N; i++) {
    vkCmdDispatch(cmd, 1, 1, 1);
}
```

## Pipeline Chains

Chain multiple operations with memory barriers:

```c
// Step 1: Add 10
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, add_pipeline);
vkCmdDispatch(cmd, workgroups, 1, 1);

// Barrier: Ensure add completes before multiply reads
VkMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
};
vkCmdPipelineBarrier(cmd,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    0, 1, &barrier, 0, NULL, 0, NULL);

// Step 2: Multiply by 2
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, multiply_pipeline);
vkCmdDispatch(cmd, workgroups, 1, 1);
```

## The Shaders

### add.comp
```glsl
#version 450
layout(local_size_x = 256) in;
layout(set = 0, binding = 0) buffer Data { float v[]; } data;
layout(push_constant) uniform PC { float value; } pc;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    data.v[idx] = data.v[idx] + pc.value;
}
```

### multiply.comp
```glsl
#version 450
layout(local_size_x = 256) in;
layout(set = 0, binding = 0) buffer Data { float v[]; } data;
layout(push_constant) uniform PC { float value; } pc;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    data.v[idx] = data.v[idx] * pc.value;
}
```

## Running the Example

```bash
./build/bin/ch04_pipelines
```

Expected output:
```
==============================================
  VKCompute - Chapter 04: Pipelines
==============================================

Creating 4 compute pipelines...
  - add.comp.spv
  - multiply.comp.spv
  - square.comp.spv
  - sqrt.comp.spv

Pipeline creation time: 12.3 ms

Running pipeline chain: (input + 10) * 2
First 5 results:
  [0] (1.0 + 10.0) * 2.0 = 22.0 (got 22.0) ✓
  [1] (2.0 + 10.0) * 2.0 = 24.0 (got 24.0) ✓
  [2] (3.0 + 10.0) * 2.0 = 26.0 (got 26.0) ✓
```

## Pipeline Derivatives

Create related pipelines faster with derivatives:

```c
// Base pipeline
VkComputePipelineCreateInfo base_info = {
    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    .flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT,
    .stage = base_stage,
    .layout = layout
};

VkPipeline base_pipeline;
vkCreateComputePipelines(device, cache, 1, &base_info, 
                         NULL, &base_pipeline);

// Derived pipeline
VkComputePipelineCreateInfo derived_info = {
    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    .flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT,
    .stage = derived_stage,
    .layout = layout,
    .basePipelineHandle = base_pipeline,
    .basePipelineIndex = -1
};
```

!!! note "Driver Dependent"
    The performance benefit of derivatives varies by driver. Profile to verify.

## Exercises

1. **Cache Persistence**: Save the pipeline cache to disk and measure loading time on restart.

2. **More Operations**: Add sqrt and power shaders, chain them in different orders.

3. **Performance Test**: Measure the cost of pipeline switches vs. dispatch count.

## Common Errors

### VK_ERROR_INVALID_SHADER_NV

Shader compilation failed:
- Check GLSL syntax
- Verify SPIR-V was generated correctly with `glslc`
- Enable validation layers for detailed errors

### Pipeline Layout Mismatch

Shader expects resources the layout doesn't provide:
- Ensure descriptor set layouts match shader `layout()` declarations
- Check push constant ranges match shader usage

## What's Next?

We've been submitting commands and hoping they complete. In [Chapter 05](ch05_synchronization.md), we'll learn proper GPU synchronization with fences, semaphores, and barriers.

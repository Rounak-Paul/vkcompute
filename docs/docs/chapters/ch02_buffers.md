# Chapter 02: Buffers

## Overview

In this chapter, you'll learn how to transfer data between CPU and GPU using Vulkan buffers:

- Create GPU buffers
- Understand memory types (host-visible vs device-local)
- Upload data to the GPU
- Run a simple compute shader
- Read results back

**What you'll learn:**

- Buffer creation and memory allocation
- Memory property flags
- Staging buffers for optimal performance
- Memory mapping

!!! info "Introducing the vkc Helper Library"
    Starting with this chapter, we use the `vkc` helper library to reduce boilerplate. Chapter 1 showed raw Vulkan to teach initialization; from now on, we wrap common patterns so we can focus on new concepts.
    
    ```c
    #include "vk_init.h"    // VkcContext, buffer helpers
    #include "vk_utils.h"   // Shader loading, descriptors
    #include "file_utils.h" // File I/O utilities
    ```
    
    The library is intentionally simple — you can read the source in the `vkc/` folder to see the raw Vulkan calls.

## GPU Memory Architecture

Before diving into code, understand that GPUs have different memory types:

```
┌─────────────────────────────────────────────────────────┐
│                         CPU                              │
│  ┌─────────────────────────────────────────────────┐    │
│  │              System RAM (Host Memory)            │    │
│  └─────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────┘
                          │
                    PCIe Bus (slower)
                          │
┌─────────────────────────────────────────────────────────┐
│                         GPU                              │
│  ┌─────────────────────────────────────────────────┐    │
│  │              VRAM (Device Local)                 │    │
│  │              - Fastest for GPU                   │    │
│  │              - Not directly CPU accessible       │    │
│  └─────────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────────┐    │
│  │              Shared Memory (Host Visible)        │    │
│  │              - CPU can read/write               │    │
│  │              - Slower for GPU compute           │    │
│  └─────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────┘
```

## Memory Types

Vulkan exposes different memory types with property flags:

| Flag | Meaning |
|------|---------|
| `DEVICE_LOCAL` | Fastest for GPU, typically VRAM |
| `HOST_VISIBLE` | CPU can map and access |
| `HOST_COHERENT` | No manual flush/invalidate needed |
| `HOST_CACHED` | CPU reads are cached (faster) |

Common combinations:

```c
// Fast GPU access, CPU can't touch directly
VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT

// CPU can write, good for uploads
VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
VK_MEMORY_PROPERTY_HOST_COHERENT_BIT

// CPU can read efficiently
VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
VK_MEMORY_PROPERTY_HOST_CACHED_BIT
```

## Creating a Buffer

Buffer creation is a two-step process:

### Step 1: Create the Buffer Object

```c
VkBufferCreateInfo buffer_info = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = 1024 * sizeof(float),  // 4 KB
    .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE
};

VkBuffer buffer;
vkCreateBuffer(device, &buffer_info, NULL, &buffer);
```

### Step 2: Allocate and Bind Memory

```c
// Query memory requirements
VkMemoryRequirements mem_reqs;
vkGetBufferMemoryRequirements(device, buffer, &mem_reqs);

// Find suitable memory type
uint32_t memory_type = find_memory_type(
    mem_reqs.memoryTypeBits,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
);

// Allocate memory
VkMemoryAllocateInfo alloc_info = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = mem_reqs.size,
    .memoryTypeIndex = memory_type
};

VkDeviceMemory memory;
vkAllocateMemory(device, &alloc_info, NULL, &memory);

// Bind memory to buffer
vkBindBufferMemory(device, buffer, memory, 0);
```

### Finding Memory Type

```c
uint32_t find_memory_type(uint32_t type_filter, 
                          VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);
    
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) 
                == properties) {
            return i;
        }
    }
    return UINT32_MAX;  // Not found
}
```

## Uploading Data

For host-visible memory, map and copy:

```c
// Map memory
void* mapped;
vkMapMemory(device, memory, 0, buffer_size, 0, &mapped);

// Copy data
memcpy(mapped, source_data, buffer_size);

// Unmap (optional if you'll write again)
vkUnmapMemory(device, memory);
```

!!! tip "Persistent Mapping"
    For frequently updated buffers, keep memory mapped. Don't map/unmap every frame.

## The Staging Buffer Pattern

For best performance, use device-local memory for compute. But CPU can't write directly to it. Solution: **staging buffer**.

```
┌──────────────────┐     ┌──────────────────┐     ┌──────────────────┐
│   CPU Memory     │────▶│  Staging Buffer  │────▶│  Device Buffer   │
│   (your data)    │copy │  (host-visible)  │GPU  │  (device-local)  │
│                  │     │                  │copy │                  │
└──────────────────┘     └──────────────────┘     └──────────────────┘
```

### Implementation

```c
// 1. Create staging buffer (host-visible)
VkBuffer staging_buffer;
VkDeviceMemory staging_memory;
create_buffer(size, 
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    &staging_buffer, &staging_memory);

// 2. Create device buffer (device-local)
VkBuffer device_buffer;
VkDeviceMemory device_memory;
create_buffer(size,
    VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    &device_buffer, &device_memory);

// 3. Copy data to staging buffer
void* mapped;
vkMapMemory(device, staging_memory, 0, size, 0, &mapped);
memcpy(mapped, source_data, size);
vkUnmapMemory(device, staging_memory);

// 4. GPU copy: staging -> device
VkCommandBuffer cmd = begin_single_time_commands();

VkBufferCopy copy_region = {
    .srcOffset = 0,
    .dstOffset = 0,
    .size = size
};
vkCmdCopyBuffer(cmd, staging_buffer, device_buffer, 1, &copy_region);

end_single_time_commands(cmd);

// 5. Staging buffer can be freed now
vkDestroyBuffer(device, staging_buffer, NULL);
vkFreeMemory(device, staging_memory, NULL);
```

## Reading Data Back

Same pattern in reverse for reading results:

```c
// GPU copy: device -> staging
vkCmdCopyBuffer(cmd, device_buffer, staging_buffer, 1, &copy_region);

// Wait for completion, then read from staging
void* mapped;
vkMapMemory(device, staging_memory, 0, size, 0, &mapped);
memcpy(dest_data, mapped, size);
vkUnmapMemory(device, staging_memory);
```

## The Compute Shader

Here's a simple shader that squares each value:

```glsl
#version 450

layout(local_size_x = 256) in;

layout(set = 0, binding = 0) buffer Data {
    float values[];
} data;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    data.values[idx] = data.values[idx] * data.values[idx];
}
```

## Running the Example

```bash
./build/bin/ch02_buffers
```

Expected output:
```
==============================================
  VKCompute - Chapter 02: Buffer Types
==============================================

=== Memory Heaps ===
  Heap 0: 8.00 GB (Device Local)
  Heap 1: 32.00 GB (Host)

=== Memory Types ===
  Type 0 (Heap 0): DEVICE_LOCAL 
  Type 1 (Heap 1): HOST_VISIBLE HOST_COHERENT 
  Type 2 (Heap 1): HOST_VISIBLE HOST_COHERENT HOST_CACHED 

Buffer size: 4.00 MB

--- Method 1: Host Visible Buffer ---
  Upload time: 0.45 ms (8.89 GB/s)

--- Method 2: Staging Buffer + Device Local ---
  Staging upload: 0.42 ms
  GPU copy time: 0.21 ms (19.05 GB/s)

--- Running Compute Shader ---
  Compute time: 0.58 ms

=== Verification ===
First 5 values:
  0² = 0 (expected 0)
  1² = 1 (expected 1)
  2² = 4 (expected 4)
  3² = 9 (expected 9)
  4² = 16 (expected 16)

All 1048576 values computed correctly!
```

## Performance Considerations

| Approach | Upload Speed | Compute Speed | Best For |
|----------|--------------|---------------|----------|
| Host-visible only | Fast | Slower | Small buffers, frequent updates |
| Staging + Device | Slower setup | Fastest | Large buffers, compute-intensive |

!!! tip "Unified Memory (Integrated GPUs)"
    On integrated GPUs (like Apple M-series), memory is often unified. Device-local memory may also be host-visible, eliminating the need for staging.

## Buffer Usage Flags

| Flag | Purpose |
|------|---------|
| `STORAGE_BUFFER_BIT` | Read/write from shaders |
| `UNIFORM_BUFFER_BIT` | Read-only constants |
| `TRANSFER_SRC_BIT` | Source for copy operations |
| `TRANSFER_DST_BIT` | Destination for copy operations |

Combine flags as needed:
```c
VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
```

## Exercises

1. **Memory Comparison**: Benchmark host-visible vs device-local memory for different buffer sizes.

2. **Persistent Mapping**: Modify the code to keep the staging buffer mapped and reuse it.

3. **Different Operations**: Change the shader to perform different operations (add, multiply, etc.).

## Common Errors

### VK_ERROR_OUT_OF_DEVICE_MEMORY

- Buffer too large for GPU
- Too many allocations (Vulkan has a limit, ~4096 on some GPUs)
- Use a memory allocator library for production code

### Data Corruption

- Forgot `HOST_COHERENT` flag — need `vkFlushMappedMemoryRanges`
- Didn't wait for GPU to finish before reading
- Buffer size mismatch between CPU and shader

## What's Next?

We can create buffers and run shaders, but how do we tell the shader where to find the buffers? In [Chapter 03](ch03_descriptors.md), we'll learn about descriptor sets — Vulkan's resource binding mechanism.

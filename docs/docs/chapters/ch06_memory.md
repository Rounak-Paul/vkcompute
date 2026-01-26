# Chapter 06: Memory

## Overview

Efficient memory management is crucial for GPU performance. This chapter covers:

- Memory heaps and types in depth
- Allocation strategies
- Memory aliasing and suballocation
- When to use different memory types

**What you'll learn:**

- Understanding GPU memory architecture
- Optimizing memory usage patterns
- Avoiding allocation overhead

## Memory Architecture

### Heaps

Physical memory pools:

```c
VkPhysicalDeviceMemoryProperties mem_props;
vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

for (uint32_t i = 0; i < mem_props.memoryHeapCount; i++) {
    VkMemoryHeap heap = mem_props.memoryHeaps[i];
    printf("Heap %u: %.2f GB", i, heap.size / 1e9);
    if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
        printf(" (Device Local)");  // VRAM
    }
    printf("\n");
}
```

Typical discrete GPU:
```
Heap 0: 8.00 GB (Device Local)  ← VRAM
Heap 1: 32.00 GB                 ← System RAM
```

Integrated GPU (or Apple Silicon):
```
Heap 0: 16.00 GB (Device Local)  ← Unified Memory
```

### Memory Types

Each type belongs to a heap and has properties:

```c
for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
    VkMemoryType type = mem_props.memoryTypes[i];
    printf("Type %u (Heap %u): ", i, type.heapIndex);
    
    if (type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        printf("DEVICE_LOCAL ");
    if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        printf("HOST_VISIBLE ");
    if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        printf("HOST_COHERENT ");
    if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
        printf("HOST_CACHED ");
}
```

## Choosing Memory Types

### For Compute Buffers (GPU Only)

```c
// Fastest GPU access, CPU can't touch
VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
```

### For Upload (CPU → GPU)

```c
// CPU writes, GPU reads
VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
```

### For Readback (GPU → CPU)

```c
// GPU writes, CPU reads with cache
VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
VK_MEMORY_PROPERTY_HOST_CACHED_BIT
```

## Memory Allocation Limits

Vulkan has a **limit on the number of allocations** (typically ~4096):

```c
VkPhysicalDeviceProperties props;
vkGetPhysicalDeviceProperties(physical_device, &props);
// props.limits.maxMemoryAllocationCount
```

!!! danger "Don't Allocate Per-Buffer"
    Creating 10,000 buffers with 10,000 allocations will fail!

## Suballocation

Allocate large blocks, suballocate within:

```
┌────────────────────────────────────────────────────────┐
│                   Large Allocation (64 MB)             │
├──────────────┬──────────────┬──────────────┬───────────┤
│   Buffer A   │   Buffer B   │   Buffer C   │   Free    │
│   (16 MB)    │   (8 MB)     │   (32 MB)    │   (8 MB)  │
└──────────────┴──────────────┴──────────────┴───────────┘
```

### Basic Suballocator

```c
typedef struct {
    VkDeviceMemory memory;
    VkDeviceSize size;
    VkDeviceSize offset;  // Current allocation point
} MemoryBlock;

VkDeviceSize suballocate(MemoryBlock* block, VkDeviceSize size, 
                         VkDeviceSize alignment) {
    // Align offset
    VkDeviceSize aligned_offset = (block->offset + alignment - 1) 
                                  & ~(alignment - 1);
    
    if (aligned_offset + size > block->size) {
        return UINT64_MAX;  // Out of space
    }
    
    VkDeviceSize result = aligned_offset;
    block->offset = aligned_offset + size;
    return result;
}
```

### Binding with Offset

```c
VkDeviceSize offset = suballocate(&block, buffer_size, alignment);
vkBindBufferMemory(device, buffer, block.memory, offset);
```

## Memory Aliasing

Multiple buffers can share the same memory (if not used simultaneously):

```c
// Allocate once
VkDeviceSize total_size = max(size_a, max(size_b, size_c));
VkDeviceMemory memory;
vkAllocateMemory(device, &alloc_info, NULL, &memory);

// Bind multiple buffers to same memory
vkBindBufferMemory(device, buffer_a, memory, 0);
vkBindBufferMemory(device, buffer_b, memory, 0);
vkBindBufferMemory(device, buffer_c, memory, 0);
```

!!! warning "Aliasing Rules"
    Only one aliased buffer can be "active" at a time. Use barriers to transition between them.

## Dedicated Allocations

Some resources benefit from dedicated memory:

```c
VkMemoryDedicatedRequirements dedicated_reqs = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS
};

VkMemoryRequirements2 mem_reqs2 = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
    .pNext = &dedicated_reqs
};

VkBufferMemoryRequirementsInfo2 buf_reqs_info = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
    .buffer = buffer
};

vkGetBufferMemoryRequirements2(device, &buf_reqs_info, &mem_reqs2);

if (dedicated_reqs.requiresDedicatedAllocation ||
    dedicated_reqs.prefersDedicatedAllocation) {
    // Use dedicated allocation
    VkMemoryDedicatedAllocateInfo dedicated_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        .buffer = buffer
    };
    alloc_info.pNext = &dedicated_info;
}
```

## Memory Budget

Query available memory (with extension):

```c
// Requires VK_EXT_memory_budget
VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT
};

VkPhysicalDeviceMemoryProperties2 props2 = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
    .pNext = &budget
};

vkGetPhysicalDeviceMemoryProperties2(physical_device, &props2);

for (uint32_t i = 0; i < props2.memoryProperties.memoryHeapCount; i++) {
    printf("Heap %u: %llu / %llu MB used\n", i,
           budget.heapUsage[i] / (1024*1024),
           budget.heapBudget[i] / (1024*1024));
}
```

## Running the Example

```bash
./build/bin/ch06_memory
```

Expected output:
```
==============================================
  VKCompute - Chapter 06: Memory
==============================================

=== Memory Architecture ===
Heaps:
  Heap 0: 8.00 GB (Device Local)
  Heap 1: 32.00 GB (Host)

Types:
  Type 0 (Heap 0): DEVICE_LOCAL
  Type 1 (Heap 1): HOST_VISIBLE HOST_COHERENT
  Type 2 (Heap 1): HOST_VISIBLE HOST_COHERENT HOST_CACHED

=== Suballocation Demo ===
Allocated 64 MB block
  Buffer 1: offset 0, size 16 MB
  Buffer 2: offset 16 MB, size 8 MB
  Buffer 3: offset 24 MB, size 32 MB
  Remaining: 8 MB

=== Performance Comparison ===
Host-visible coherent: 0.07 ms per 4MB (56 GB/s)
Staging + device-local: 0.21 ms per 4MB (19 GB/s copy)
  But compute is 3x faster on device-local!
```

## Best Practices

### Do

- ✅ Suballocate from large blocks
- ✅ Use device-local for compute-heavy buffers
- ✅ Keep staging buffers mapped
- ✅ Pool similar allocations together

### Don't

- ❌ Allocate per-buffer
- ❌ Exceed `maxMemoryAllocationCount`
- ❌ Map/unmap repeatedly (keep mapped)
- ❌ Use host-visible for compute-intensive data

## Memory Allocator Libraries

For production, use a library:

- **VMA (Vulkan Memory Allocator)**: AMD's widely-used allocator
- **D3D12MA**: Can be adapted for Vulkan concepts

```c
// VMA example
VmaAllocatorCreateInfo allocator_info = {...};
VmaAllocator allocator;
vmaCreateAllocator(&allocator_info, &allocator);

VmaAllocationCreateInfo alloc_ci = {
    .usage = VMA_MEMORY_USAGE_GPU_ONLY
};
vmaCreateBuffer(allocator, &buffer_info, &alloc_ci, 
                &buffer, &allocation, NULL);
```

## Exercises

1. **Allocation Limit**: Create buffers until you hit the allocation limit, then implement suballocation.

2. **Memory Pressure**: Allocate more than available VRAM and observe fallback to system memory.

3. **Benchmark**: Compare compute performance on host-visible vs device-local memory.

## Common Errors

### VK_ERROR_OUT_OF_DEVICE_MEMORY

- Check memory budget before allocating
- Implement suballocation
- Consider smaller working sets

### Wrong Memory Type

Mapped memory returns garbage:
- Ensure `HOST_VISIBLE` flag
- Check `HOST_COHERENT` or flush manually

### Alignment Violations

Buffer binding fails:
- Respect `memoryRequirements.alignment`
- Some buffers need stricter alignment (uniform buffers especially)

## What's Next?

We've been using descriptors to pass data to shaders. In [Chapter 07](ch07_push_constants.md), we'll learn about push constants — a faster way to pass small amounts of data.

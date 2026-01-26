# Chapter 05: Synchronization

## Overview

GPUs execute commands asynchronously. This chapter covers how to synchronize:

- CPU-GPU synchronization with fences
- GPU-GPU synchronization with semaphores
- Memory dependencies with barriers

**What you'll learn:**

- When and why synchronization is needed
- Choosing the right synchronization primitive
- Avoiding race conditions and data hazards

## Why Synchronization?

```
CPU                          GPU
 │                            │
 ├── Submit commands ────────▶│
 │   (returns immediately)    ├── Process cmd 1
 │                            ├── Process cmd 2
 ├── Read results? ◀──────────┤── Still working!
 │   (WRONG!)                 │
 │                            ├── Done
 ├── Wait for fence ◀─────────┤
 ├── Read results ✓           │
```

Without synchronization:
- Reading buffer while GPU is writing → garbage data
- Submitting new work before previous completes → undefined behavior
- Overwriting resources in use → corruption

## Synchronization Primitives

| Primitive | Scope | Purpose |
|-----------|-------|---------|
| **Fence** | CPU ↔ GPU | Know when GPU work completes |
| **Semaphore** | GPU ↔ GPU | Order queue submissions |
| **Barrier** | Within command buffer | Order operations within a submit |
| **Event** | Fine-grained | Split barrier across commands |

## Fences: CPU-GPU Sync

### Creating a Fence

```c
VkFenceCreateInfo fence_info = {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .flags = 0  // Start unsignaled
};

VkFence fence;
vkCreateFence(device, &fence_info, NULL, &fence);
```

### Submit with Fence

```c
VkSubmitInfo submit_info = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers = &cmd
};

vkQueueSubmit(queue, 1, &submit_info, fence);
```

### Wait for Completion

```c
// Block until GPU signals fence
vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

// Reset for reuse
vkResetFences(device, 1, &fence);
```

### Polling (Non-blocking)

```c
VkResult result = vkGetFenceStatus(device, fence);
if (result == VK_SUCCESS) {
    // Work completed
} else if (result == VK_NOT_READY) {
    // Still in progress
}
```

## Semaphores: GPU-GPU Sync

Semaphores order operations between queue submissions:

```c
VkSemaphoreCreateInfo sem_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
};

VkSemaphore semaphore;
vkCreateSemaphore(device, &sem_info, NULL, &semaphore);
```

### Signal and Wait

```c
// Submission 1: Signal when done
VkSubmitInfo submit1 = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers = &cmd1,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &semaphore
};
vkQueueSubmit(queue, 1, &submit1, VK_NULL_HANDLE);

// Submission 2: Wait before starting
VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
VkSubmitInfo submit2 = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &semaphore,
    .pWaitDstStageMask = &wait_stage,
    .commandBufferCount = 1,
    .pCommandBuffers = &cmd2
};
vkQueueSubmit(queue, 1, &submit2, fence);
```

## Pipeline Barriers: Memory Dependencies

Barriers ensure memory operations complete before subsequent operations start.

### When Do You Need Barriers?

| Scenario | Barrier Needed? |
|----------|-----------------|
| Read after write (same buffer) | Yes |
| Write after read | Yes |
| Write after write | Yes |
| Read after read | No |
| Different buffers, no dependency | No |

### Memory Barrier

```c
VkMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
};

vkCmdPipelineBarrier(cmd,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // After this stage
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // Before this stage
    0,                                      // Flags
    1, &barrier,                           // Memory barriers
    0, NULL,                               // Buffer barriers
    0, NULL);                              // Image barriers
```

### Buffer Memory Barrier

For specific buffer ranges:

```c
VkBufferMemoryBarrier buffer_barrier = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .buffer = buffer,
    .offset = 0,
    .size = VK_WHOLE_SIZE
};

vkCmdPipelineBarrier(cmd,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    0, 0, NULL,
    1, &buffer_barrier,
    0, NULL);
```

## Access Masks

Common access flags:

| Flag | Usage |
|------|-------|
| `SHADER_READ_BIT` | Read from shader (storage/uniform buffer) |
| `SHADER_WRITE_BIT` | Write from shader (storage buffer) |
| `TRANSFER_READ_BIT` | Source of copy operation |
| `TRANSFER_WRITE_BIT` | Destination of copy operation |
| `HOST_READ_BIT` | CPU read after mapping |
| `HOST_WRITE_BIT` | CPU write after mapping |

## Pipeline Stages

Common stage flags for compute:

| Flag | When |
|------|------|
| `TOP_OF_PIPE_BIT` | Very beginning |
| `COMPUTE_SHADER_BIT` | Compute shader execution |
| `TRANSFER_BIT` | Copy operations |
| `HOST_BIT` | CPU access |
| `BOTTOM_OF_PIPE_BIT` | Very end |

## Multiple Independent Dispatches

When dispatches are independent, submit them together:

```c
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

// These write to different buffers - no barrier needed between
vkCmdBindDescriptorSets(cmd, ..., &set_a, ...);
vkCmdDispatch(cmd, 256, 1, 1);

vkCmdBindDescriptorSets(cmd, ..., &set_b, ...);
vkCmdDispatch(cmd, 256, 1, 1);

vkCmdBindDescriptorSets(cmd, ..., &set_c, ...);
vkCmdDispatch(cmd, 256, 1, 1);

// One barrier after all, before reading any results
vkCmdPipelineBarrier(cmd, ...);
```

## Running the Example

```bash
./build/bin/ch05_synchronization
```

Expected output:
```
==============================================
  VKCompute - Chapter 05: Synchronization
==============================================

=== Part 1: Basic Fence ===
Submitting compute work...
Waiting for fence...
  Completed in 0.15 ms

=== Part 2: Chained Dispatches ===
Running 3 sequential operations with barriers...
  Operation 1: increment
  [barrier]
  Operation 2: double
  [barrier]
  Operation 3: square
Result: (1 + 1) * 2 = 4, 4² = 16 ✓

=== Part 3: Multiple Submissions ===
Submitting 3 independent operations...
  All completed in 0.41 ms
```

## Common Patterns

### Wait Idle (Simple but Slow)

```c
// Wait for everything to finish
vkQueueWaitIdle(queue);
// or
vkDeviceWaitIdle(device);
```

!!! warning "Performance"
    `WaitIdle` is simple but prevents CPU-GPU overlap. Use fences for production code.

### Double Buffering

Overlap CPU and GPU work:

```c
VkFence fences[2];
VkCommandBuffer cmds[2];
int frame = 0;

while (running) {
    // Wait for previous frame's GPU work
    vkWaitForFences(device, 1, &fences[frame], VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &fences[frame]);
    
    // Record new commands (CPU work)
    record_commands(cmds[frame]);
    
    // Submit (starts GPU work)
    vkQueueSubmit(queue, 1, &submit, fences[frame]);
    
    frame = 1 - frame;  // Alternate
}
```

## Exercises

1. **Fence Timing**: Measure actual GPU execution time by recording timestamps before and after fence wait.

2. **Parallel Reduction**: Implement a multi-pass reduction with proper barriers between passes.

3. **Producer-Consumer**: Set up two queues where one produces data and another consumes it.

## Common Errors

### Data Race

Symptoms: Intermittent wrong results, works sometimes
```c
// Missing barrier!
vkCmdDispatch(cmd, 64, 1, 1);  // Writes to buffer
vkCmdDispatch(cmd, 64, 1, 1);  // Reads same buffer - RACE!
```

### Deadlock

Waiting on fence that was never submitted:
```c
vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);  // Hangs!
// Forgot to call vkQueueSubmit with this fence
```

### Wrong Stage Mask

Barrier doesn't actually synchronize:
```c
vkCmdPipelineBarrier(cmd,
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  // Too early!
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    ...);
```

## What's Next?

We've touched on memory types in Chapter 02. In [Chapter 06](ch06_memory.md), we'll go deeper into Vulkan's memory model and advanced allocation strategies.

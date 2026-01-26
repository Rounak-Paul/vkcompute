# Introduction to Vulkan Compute

## What is Vulkan?

Vulkan is a modern, low-level graphics and compute API developed by the Khronos Group. Unlike higher-level APIs, Vulkan gives you explicit control over the GPU, resulting in better performance but requiring more code.

## Graphics vs Compute

Vulkan supports two main types of workloads:

| Aspect | Graphics | Compute |
|--------|----------|---------|
| Purpose | Rendering images | General-purpose processing |
| Pipeline | Complex (vertex, fragment, etc.) | Simple (single stage) |
| Output | Framebuffer/images | Buffers/images |
| Use cases | Games, visualization | ML, physics, image processing |

This series focuses entirely on **Compute** — using the GPU as a massively parallel processor.

## The Vulkan Execution Model

### 1. Host and Device

```
┌─────────────────────────────────────────────────────────┐
│                         HOST (CPU)                       │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐  │
│  │ Application │───▶│   Vulkan    │───▶│   Driver    │  │
│  │   (Your C)  │    │   Library   │    │             │  │
│  └─────────────┘    └─────────────┘    └─────────────┘  │
└─────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────┐
│                        DEVICE (GPU)                      │
│  ┌─────────────────────────────────────────────────┐    │
│  │                  Compute Units                   │    │
│  │  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐       │    │
│  │  │ CU  │ │ CU  │ │ CU  │ │ CU  │ │ ... │       │    │
│  │  └─────┘ └─────┘ └─────┘ └─────┘ └─────┘       │    │
│  └─────────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────────┐    │
│  │                    GPU Memory                    │    │
│  └─────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────┘
```

- **Host**: Your CPU and system memory where your C code runs
- **Device**: The GPU with its own memory and compute units

### 2. Queues and Commands

The GPU doesn't execute code directly. Instead, you:

1. Record commands into a **command buffer**
2. Submit the command buffer to a **queue**
3. The GPU processes commands asynchronously

```c
// Record commands
vkBeginCommandBuffer(cmd, &begin_info);
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
vkCmdDispatch(cmd, group_count_x, 1, 1);
vkEndCommandBuffer(cmd);

// Submit to queue
vkQueueSubmit(queue, 1, &submit_info, fence);

// Wait for completion
vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
```

### 3. Workgroups and Invocations

Compute shaders run in a hierarchical structure:

```
Dispatch (vkCmdDispatch)
│
├── Workgroup [0,0,0]
│   ├── Invocation (0,0,0)
│   ├── Invocation (1,0,0)
│   ├── Invocation (2,0,0)
│   └── ... up to local_size
│
├── Workgroup [1,0,0]
│   └── ...
│
└── Workgroup [N,0,0]
    └── ...
```

- **Invocation**: A single execution of your shader (like a thread)
- **Workgroup**: A group of invocations that can share memory and synchronize
- **Dispatch**: The total number of workgroups to launch

## Key Vulkan Objects

Here's what you'll create in this series:

| Object | Purpose | Created In |
|--------|---------|------------|
| `VkInstance` | Connection to Vulkan library | Chapter 1 |
| `VkPhysicalDevice` | Represents a GPU | Chapter 1 |
| `VkDevice` | Logical device for commands | Chapter 1 |
| `VkQueue` | Submits work to GPU | Chapter 1 |
| `VkBuffer` | GPU memory storage | Chapter 2 |
| `VkDescriptorSet` | Binds resources to shaders | Chapter 3 |
| `VkPipeline` | Compiled shader + state | Chapter 4 |
| `VkFence` | CPU-GPU synchronization | Chapter 5 |
| `VkCommandBuffer` | Records GPU commands | Chapter 5 |

## Compute Shaders (GLSL)

Shaders are written in GLSL and compiled to SPIR-V:

```glsl
#version 450

// Workgroup size: 256 threads
layout(local_size_x = 256) in;

// Input/output buffers
layout(set = 0, binding = 0) readonly buffer Input {
    float data[];
} input_buf;

layout(set = 0, binding = 1) writeonly buffer Output {
    float data[];
} output_buf;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    output_buf.data[idx] = input_buf.data[idx] * 2.0;
}
```

Key concepts:

- `local_size_x`: Threads per workgroup
- `gl_GlobalInvocationID`: Unique index for this invocation
- `layout(set, binding)`: Where to find resources

## The Vulkan Workflow

Every Vulkan compute program follows this pattern:

```
1. Initialize
   └── Create instance, device, queue

2. Setup Resources
   ├── Allocate buffers
   ├── Create descriptor sets
   └── Load/create pipeline

3. Execute
   ├── Record command buffer
   ├── Submit to queue
   └── Wait for completion

4. Cleanup
   └── Destroy all objects
```

## Error Handling

Vulkan functions return `VkResult`. Always check for success:

```c
VkResult result = vkCreateBuffer(device, &info, NULL, &buffer);
if (result != VK_SUCCESS) {
    fprintf(stderr, "Failed to create buffer: %d\n", result);
    return -1;
}
```

## Validation Layers

Vulkan has minimal error checking by default for performance. Enable **validation layers** during development:

```c
const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
VkInstanceCreateInfo create_info = {
    // ...
    .enabledLayerCount = 1,
    .ppEnabledLayerNames = layers
};
```

Validation layers catch:

- Invalid API usage
- Memory leaks
- Synchronization errors
- Best practice violations

## What's Next?

Now that you understand the concepts, let's write code! Head to [Chapter 01](chapters/ch01_hello_compute.md) to create your first Vulkan program.

!!! info "Don't Worry"
    Vulkan has a lot of concepts, but you'll learn them incrementally. Each chapter introduces just a few new ideas, building on what you've already learned.

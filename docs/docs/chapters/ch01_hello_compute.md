# Chapter 01: Hello Vulkan Compute

## Overview

In this chapter, you'll create your first Vulkan program. We won't run any compute shaders yet — the goal is to understand the initialization process:

- Create a Vulkan instance
- Enumerate and select a GPU
- Create a logical device with a compute queue

**What you'll learn:**

- The Vulkan object hierarchy
- Physical vs logical devices
- Queue families and compute capability

## The Code

Let's walk through the complete program:

### Includes and Setup

```c
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>
```

We include the Vulkan header directly. No helper libraries yet — we want to see every Vulkan call.

### Step 1: Create a Vulkan Instance

The instance is your connection to the Vulkan library:

```c
VkApplicationInfo app_info = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = "CH01 Hello Compute",
    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
    .pEngineName = "No Engine",
    .engineVersion = VK_MAKE_VERSION(1, 0, 0),
    .apiVersion = VK_API_VERSION_1_2
};

VkInstanceCreateInfo instance_info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &app_info
};

VkInstance instance;
VkResult result = vkCreateInstance(&instance_info, NULL, &instance);
if (result != VK_SUCCESS) {
    fprintf(stderr, "Failed to create Vulkan instance!\n");
    return 1;
}
printf("[ok] Vulkan instance created\n");
```

!!! note "sType Pattern"
    Every Vulkan structure has an `sType` field that identifies its type. This enables the driver to validate and extend structures.

!!! warning "macOS Note"
    On macOS with MoltenVK, you need the portability extension. The full code handles this with `#ifdef __APPLE__`.

### Step 2: Find a Physical Device (GPU)

Enumerate available GPUs:

```c
uint32_t device_count = 0;
vkEnumeratePhysicalDevices(instance, &device_count, NULL);

if (device_count == 0) {
    fprintf(stderr, "No GPUs with Vulkan support found!\n");
    return 1;
}

VkPhysicalDevice* devices = malloc(device_count * sizeof(VkPhysicalDevice));
vkEnumeratePhysicalDevices(instance, &device_count, devices);
```

The two-call pattern is common in Vulkan:

1. First call with `NULL` to get the count
2. Allocate memory
3. Second call to fill the array

### Step 3: Query Device Properties

For each GPU, get its properties and find compute queues:

```c
for (uint32_t i = 0; i < device_count; i++) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(devices[i], &props);
    
    printf("GPU %u: %s\n", i, props.deviceName);
    printf("  Type: %s\n", 
        props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU 
            ? "Discrete GPU" : "Other");
    printf("  API Version: %u.%u.%u\n",
        VK_VERSION_MAJOR(props.apiVersion),
        VK_VERSION_MINOR(props.apiVersion),
        VK_VERSION_PATCH(props.apiVersion));
}
```

### Step 4: Find a Compute Queue Family

GPUs organize their queues into families with different capabilities:

```c
uint32_t queue_family_count = 0;
vkGetPhysicalDeviceQueueFamilyProperties(physical_device, 
                                          &queue_family_count, NULL);

VkQueueFamilyProperties* queue_families = 
    malloc(queue_family_count * sizeof(VkQueueFamilyProperties));
vkGetPhysicalDeviceQueueFamilyProperties(physical_device, 
                                          &queue_family_count, 
                                          queue_families);

uint32_t compute_queue_family = UINT32_MAX;
for (uint32_t j = 0; j < queue_family_count; j++) {
    if (queue_families[j].queueFlags & VK_QUEUE_COMPUTE_BIT) {
        compute_queue_family = j;
        break;
    }
}
```

We look for any queue family that supports `VK_QUEUE_COMPUTE_BIT`.

### Step 5: Create a Logical Device

The logical device is your interface to the GPU:

```c
float queue_priority = 1.0f;
VkDeviceQueueCreateInfo queue_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    .queueFamilyIndex = compute_queue_family,
    .queueCount = 1,
    .pQueuePriorities = &queue_priority
};

VkDeviceCreateInfo device_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .queueCreateInfoCount = 1,
    .pQueueCreateInfos = &queue_info
};

VkDevice device;
result = vkCreateDevice(physical_device, &device_info, NULL, &device);
```

### Step 6: Get the Queue Handle

```c
VkQueue compute_queue;
vkGetDeviceQueue(device, compute_queue_family, 0, &compute_queue);
printf("[ok] Compute queue retrieved\n");
```

Now we have everything needed to submit compute work!

### Cleanup

Always destroy objects in reverse order of creation:

```c
vkDestroyDevice(device, NULL);
vkDestroyInstance(instance, NULL);
```

## Running the Example

```bash
./build/bin/ch01_hello_compute
```

Expected output:
```
==============================================
  VKCompute - Chapter 01: Hello Vulkan Compute
==============================================

[ok] Vulkan instance created
[ok] Found 1 GPU(s)

  GPU 0: NVIDIA GeForce RTX 3080
    Type: Discrete GPU
    API Version: 1.3.277
    Queue family 0: supports COMPUTE (16 queues)

[ok] Selected GPU with compute queue family 0
[ok] Logical device created
[ok] Compute queue retrieved

=== SUCCESS ===
Vulkan is initialized and ready for compute work!
```

## Key Concepts

### Object Hierarchy

```
VkInstance
    └── VkPhysicalDevice (GPU hardware)
            └── VkDevice (logical interface)
                    └── VkQueue (command submission)
```

### Physical vs Logical Device

| Aspect | Physical Device | Logical Device |
|--------|-----------------|----------------|
| Represents | Actual GPU hardware | Your session with GPU |
| Count | One per GPU | Multiple possible |
| Created by | Vulkan driver | Your code |
| Contains | Properties, limits | Queues, resources |

### Queue Families

GPUs have different queue types:

- **Compute**: General computation (`VK_QUEUE_COMPUTE_BIT`)
- **Graphics**: Rendering (`VK_QUEUE_GRAPHICS_BIT`)
- **Transfer**: Memory copies (`VK_QUEUE_TRANSFER_BIT`)
- **Sparse**: Sparse memory (`VK_QUEUE_SPARSE_BINDING_BIT`)

For compute, we only need `VK_QUEUE_COMPUTE_BIT`.

## Exercises

1. **Multiple GPUs**: If you have multiple GPUs, modify the code to list all of them and let the user choose.

2. **Queue Exploration**: Print all queue families and their capabilities, not just compute.

3. **Device Features**: Query and print `VkPhysicalDeviceFeatures` to see what your GPU supports.

## Common Errors

### VK_ERROR_INCOMPATIBLE_DRIVER (-9)

On macOS, you need the portability extension:
```c
const char* extensions[] = {VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME};
instance_info.enabledExtensionCount = 1;
instance_info.ppEnabledExtensionNames = extensions;
instance_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
```

### VK_ERROR_INITIALIZATION_FAILED (-3)

- Check that Vulkan drivers are installed
- Update GPU drivers
- Verify Vulkan SDK installation with `vulkaninfo`

## What's Next?

We have a Vulkan device and queue, but we haven't done any actual computation yet. In [Chapter 02](ch02_buffers.md), we'll create buffers to send data to the GPU.

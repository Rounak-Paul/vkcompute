# Chapter 03: Descriptors

## Overview

Descriptors are how shaders access resources like buffers and images. This chapter covers:

- Descriptor set layouts
- Descriptor pools
- Allocating and updating descriptor sets
- Binding multiple resources

**What you'll learn:**

- The descriptor binding model
- How to organize resources into sets
- Updating descriptors at runtime

## The Descriptor Model

Shaders don't access buffers directly. Instead, they use **bindings**:

```glsl
layout(set = 0, binding = 0) uniform Params { ... } params;
layout(set = 0, binding = 1) buffer Input { ... } input_data;
layout(set = 1, binding = 0) buffer Output { ... } output_data;
```

The Vulkan side must match:

```
Descriptor Set 0
├── Binding 0: Uniform Buffer (params)
└── Binding 1: Storage Buffer (input)

Descriptor Set 1
└── Binding 0: Storage Buffer (output)
```

## Why Multiple Sets?

Organize by update frequency:

| Set | Contents | Update Frequency |
|-----|----------|------------------|
| 0 | Global settings | Once per frame |
| 1 | Material data | Per material |
| 2 | Object data | Per object |

This allows efficient updates without recreating everything.

## Step 1: Descriptor Set Layout

Define what resources each set contains:

```c
// Set 0: Parameters (uniform buffer)
VkDescriptorSetLayoutBinding param_binding = {
    .binding = 0,
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
};

VkDescriptorSetLayoutCreateInfo layout_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = 1,
    .pBindings = &param_binding
};

VkDescriptorSetLayout param_layout;
vkCreateDescriptorSetLayout(device, &layout_info, NULL, &param_layout);
```

```c
// Set 1: Input/Output (storage buffers)
VkDescriptorSetLayoutBinding data_bindings[2] = {
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

layout_info.bindingCount = 2;
layout_info.pBindings = data_bindings;

VkDescriptorSetLayout data_layout;
vkCreateDescriptorSetLayout(device, &layout_info, NULL, &data_layout);
```

## Step 2: Pipeline Layout

Combine set layouts for the pipeline:

```c
VkDescriptorSetLayout set_layouts[] = {param_layout, data_layout};

VkPipelineLayoutCreateInfo pipeline_layout_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 2,
    .pSetLayouts = set_layouts
};

VkPipelineLayout pipeline_layout;
vkCreatePipelineLayout(device, &pipeline_layout_info, 
                       NULL, &pipeline_layout);
```

## Step 3: Descriptor Pool

Pools allocate descriptor sets. Specify how many of each type you need:

```c
VkDescriptorPoolSize pool_sizes[] = {
    {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1
    },
    {
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 2
    }
};

VkDescriptorPoolCreateInfo pool_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .maxSets = 2,  // Total sets we can allocate
    .poolSizeCount = 2,
    .pPoolSizes = pool_sizes
};

VkDescriptorPool pool;
vkCreateDescriptorPool(device, &pool_info, NULL, &pool);
```

## Step 4: Allocate Descriptor Sets

```c
VkDescriptorSetLayout alloc_layouts[] = {param_layout, data_layout};

VkDescriptorSetAllocateInfo alloc_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = pool,
    .descriptorSetCount = 2,
    .pSetLayouts = alloc_layouts
};

VkDescriptorSet sets[2];
vkAllocateDescriptorSets(device, &alloc_info, sets);

VkDescriptorSet param_set = sets[0];
VkDescriptorSet data_set = sets[1];
```

## Step 5: Update Descriptor Sets

Point descriptors to actual buffers:

```c
// Update params set (set 0)
VkDescriptorBufferInfo param_buffer_info = {
    .buffer = param_buffer,
    .offset = 0,
    .range = VK_WHOLE_SIZE
};

VkWriteDescriptorSet param_write = {
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .dstSet = param_set,
    .dstBinding = 0,
    .dstArrayElement = 0,
    .descriptorCount = 1,
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .pBufferInfo = &param_buffer_info
};

// Update data set (set 1)
VkDescriptorBufferInfo data_buffer_infos[2] = {
    {.buffer = input_buffer, .offset = 0, .range = VK_WHOLE_SIZE},
    {.buffer = output_buffer, .offset = 0, .range = VK_WHOLE_SIZE}
};

VkWriteDescriptorSet data_writes[2] = {
    {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = data_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &data_buffer_infos[0]
    },
    {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = data_set,
        .dstBinding = 1,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &data_buffer_infos[1]
    }
};

// Apply all updates
VkWriteDescriptorSet all_writes[3] = {param_write, data_writes[0], data_writes[1]};
vkUpdateDescriptorSets(device, 3, all_writes, 0, NULL);
```

## Step 6: Bind and Dispatch

```c
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
    pipeline_layout,
    0,              // First set index
    2,              // Number of sets
    sets,           // Array of sets
    0, NULL);       // Dynamic offsets

vkCmdDispatch(cmd, workgroup_count, 1, 1);
```

## The Shader

```glsl
#version 450

layout(local_size_x = 256) in;

// Set 0: Parameters
layout(set = 0, binding = 0) uniform Params {
    float scale;
    float offset;
} params;

// Set 1: Data
layout(set = 0, binding = 0) readonly buffer Input {
    float data[];
} input_buf;

layout(set = 0, binding = 1) writeonly buffer Output {
    float data[];
} output_buf;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    output_buf.data[idx] = input_buf.data[idx] * params.scale + params.offset;
}
```

## Running the Example

```bash
./build/bin/ch03_descriptors
```

Expected output:
```
==============================================
  VKCompute - Chapter 03: Descriptor Sets
==============================================

Creating buffers...
  Input: [0, 1, 2, ...]
  Params: scale=2.0, offset=10.0

Creating descriptor layouts...
  Set 0: Uniform buffer (parameters)
  Set 1: Storage buffers (input, output)

Creating descriptor pool...
Allocating descriptor sets...
Updating descriptor sets...

Executing compute shader...
  Dispatching 4096 workgroups

First 5 results:
  0 * 2.0 + 10.0 = 10.0 (got 10.0) ✓
  1 * 2.0 + 10.0 = 12.0 (got 12.0) ✓
  2 * 2.0 + 10.0 = 14.0 (got 14.0) ✓
  3 * 2.0 + 10.0 = 16.0 (got 16.0) ✓
  4 * 2.0 + 10.0 = 18.0 (got 18.0) ✓
```

## Descriptor Types

| Type | Use Case | Shader Declaration |
|------|----------|-------------------|
| `UNIFORM_BUFFER` | Small, read-only data | `uniform Block { }` |
| `STORAGE_BUFFER` | Large, read/write data | `buffer Block { }` |
| `STORAGE_IMAGE` | Read/write images | `image2D` |
| `SAMPLED_IMAGE` | Textures with sampling | `texture2D` |
| `SAMPLER` | Texture samplers | `sampler` |

## Updating Descriptors at Runtime

You can update descriptor sets between dispatches:

```c
// Change parameters for next dispatch
params.scale = 0.5;
params.offset = -100.0;
upload_buffer(param_buffer, &params, sizeof(params));

// Re-run shader with new parameters
vkCmdDispatch(cmd, workgroup_count, 1, 1);
```

!!! warning "Update Timing"
    Don't update a descriptor set while it's in use by the GPU. Either wait for completion or use multiple sets.

## Exercises

1. **Dynamic Parameters**: Create a loop that runs the shader multiple times with different scale/offset values.

2. **Multiple Outputs**: Add a second output buffer that stores `input * scale` (without offset).

3. **Array of Buffers**: Use `descriptorCount > 1` to bind an array of buffers.

## Common Errors

### VK_ERROR_OUT_OF_POOL_MEMORY

Pool doesn't have enough descriptors:
```c
// Make sure pool sizes match your needs
pool_sizes[0].descriptorCount = 10;  // Room for 10 uniform buffers
```

### Descriptor Not Updated

Shader reads garbage:
- Check that you called `vkUpdateDescriptorSets`
- Verify buffer binding matches shader layout

### Set/Binding Mismatch

Shader expects `set=1, binding=0` but you bound to `set=0`:
- Double-check shader `layout()` declarations
- Verify `dstSet` and `dstBinding` in write structs

## What's Next?

We've been creating pipelines without much explanation. In [Chapter 04](ch04_pipelines.md), we'll dive deep into compute pipelines — how they're created, cached, and managed.

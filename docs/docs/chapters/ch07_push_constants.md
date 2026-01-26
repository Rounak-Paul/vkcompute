# Chapter 07: Push Constants

## Overview

Push constants are small pieces of data sent directly with draw/dispatch commands. This chapter covers:

- When to use push constants vs descriptors
- Size limitations
- Performance benefits

**What you'll learn:**

- Declaring push constants in shaders
- Setting up push constant ranges
- Updating push constants efficiently

## Push Constants vs Descriptors

| Aspect | Push Constants | Descriptors |
|--------|---------------|-------------|
| Size | Small (128-256 bytes typical) | Unlimited |
| Update cost | Very low | Moderate |
| Setup | Simple | Complex |
| Use case | Per-dispatch params | Buffers, images |

## How Push Constants Work

```
┌─────────────────────────────────────────────────────────┐
│                    Command Buffer                        │
├─────────────────────────────────────────────────────────┤
│  vkCmdPushConstants(cmd, layout, stages, 0, 16, &data)  │
│  vkCmdDispatch(cmd, 64, 1, 1)                           │
│                                                          │
│  vkCmdPushConstants(cmd, layout, stages, 0, 16, &data2) │
│  vkCmdDispatch(cmd, 64, 1, 1)                           │
└─────────────────────────────────────────────────────────┘
```

Data travels **with the command** — no buffer allocation, no descriptor updates.

## Size Limits

Query your device's limit:

```c
VkPhysicalDeviceProperties props;
vkGetPhysicalDeviceProperties(physical_device, &props);
printf("Max push constant size: %u bytes\n", 
       props.limits.maxPushConstantsSize);
```

| GPU | Typical Limit |
|-----|---------------|
| NVIDIA | 256 bytes |
| AMD | 128 bytes |
| Intel | 128 bytes |
| Apple | 4096 bytes |

!!! tip "Portable Code"
    Assume 128 bytes for maximum compatibility.

## Shader Declaration

```glsl
#version 450

layout(local_size_x = 256) in;

layout(push_constant) uniform PushConstants {
    float scale;
    float offset;
    uint count;
    uint pad;  // Align to 16 bytes
} pc;

layout(set = 0, binding = 0) buffer Data { float v[]; } data;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx < pc.count) {
        data.v[idx] = data.v[idx] * pc.scale + pc.offset;
    }
}
```

## Pipeline Layout Setup

```c
VkPushConstantRange push_range = {
    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
    .offset = 0,
    .size = sizeof(PushConstants)  // 16 bytes
};

VkPipelineLayoutCreateInfo layout_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts = &desc_layout,
    .pushConstantRangeCount = 1,
    .pPushConstantRanges = &push_range
};

VkPipelineLayout pipeline_layout;
vkCreatePipelineLayout(device, &layout_info, NULL, &pipeline_layout);
```

## Updating Push Constants

```c
typedef struct {
    float scale;
    float offset;
    uint32_t count;
    uint32_t pad;
} PushConstants;

PushConstants pc = {
    .scale = 2.0f,
    .offset = 10.0f,
    .count = 1024,
    .pad = 0
};

vkCmdPushConstants(cmd, pipeline_layout, 
                   VK_SHADER_STAGE_COMPUTE_BIT,
                   0, sizeof(pc), &pc);
vkCmdDispatch(cmd, 4, 1, 1);
```

## Partial Updates

Update only part of the push constant block:

```c
// Update just scale (offset 0, size 4)
float new_scale = 3.0f;
vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT,
                   0, sizeof(float), &new_scale);

// Update just offset (offset 4, size 4)  
float new_offset = 20.0f;
vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT,
                   sizeof(float), sizeof(float), &new_offset);
```

## Multiple Dispatches with Different Constants

```c
for (int i = 0; i < 100; i++) {
    PushConstants pc = {
        .scale = (float)i,
        .offset = (float)(i * 10),
        .count = ARRAY_SIZE
    };
    
    vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(pc), &pc);
    vkCmdDispatch(cmd, workgroups, 1, 1);
}
```

This is **much faster** than updating uniform buffers 100 times!

## Running the Example

```bash
./build/bin/ch07_push_constants
```

Expected output:
```
==============================================
  VKCompute - Chapter 07: Push Constants
==============================================

Max push constants size: 256 bytes

Running transform: output = input * scale + offset
  scale = 2.0, offset = 10.0

Results:
  input[0] = 0.0 -> output = 10.0 ✓
  input[1] = 1.0 -> output = 12.0 ✓
  input[2] = 2.0 -> output = 14.0 ✓

=== Performance Comparison ===
Running 1000 iterations...
  Push constants: 169 ms (0.169 ms/iter)
  Uniform buffer: 423 ms (0.423 ms/iter)
  
Push constants are 2.5x faster for frequent updates!
```

## Chained Operations

Use push constants to parameterize pipeline chains:

```c
// First: multiply
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, multiply_pipeline);
float multiplier = 2.0f;
vkCmdPushConstants(cmd, layout, stages, 0, 4, &multiplier);
vkCmdDispatch(cmd, workgroups, 1, 1);

// Barrier
vkCmdPipelineBarrier(cmd, ...);

// Second: square
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, square_pipeline);
vkCmdDispatch(cmd, workgroups, 1, 1);
```

## Alignment Rules

Push constants follow std430 layout rules:

| Type | Size | Alignment |
|------|------|-----------|
| `float` | 4 | 4 |
| `vec2` | 8 | 8 |
| `vec3` | 12 | 16 |
| `vec4` | 16 | 16 |
| `mat4` | 64 | 16 |
| `int` | 4 | 4 |
| `uint` | 4 | 4 |

```c
// C struct must match shader layout!
typedef struct {
    float scale;     // offset 0
    float offset;    // offset 4
    float padding[2]; // offset 8-15 (vec3 would need this)
    float vector[4]; // offset 16 (vec4)
} PushConstants;
```

## When to Use Push Constants

### Good Use Cases

- ✅ Transform matrices (if size allows)
- ✅ Frame/time uniforms
- ✅ Per-dispatch parameters
- ✅ Array indices
- ✅ Flags/modes

### Bad Use Cases

- ❌ Large arrays
- ❌ Texture/buffer references (use descriptors)
- ❌ Data larger than limit

## Exercises

1. **Animation**: Update a time value each frame via push constants and visualize it.

2. **Multi-pass with Parameters**: Implement a blur with push constants for direction and radius.

3. **Benchmark**: Compare push constants vs uniform buffers for various update frequencies.

## Common Errors

### Offset/Size Mismatch

```c
// Wrong: offset + size exceeds declared range
vkCmdPushConstants(cmd, layout, stages, 64, 128, &data);
// If range was declared as 128 bytes starting at 0, this fails
```

### Alignment Issues

C struct doesn't match shader layout:
```c
// Wrong
struct { float x; vec3 v; };  // v at offset 4
// Shader expects v at offset 16 (vec3 has 16-byte alignment)
```

### Stage Mismatch

Push constants to wrong stage:
```c
// Declared for COMPUTE_BIT but using VERTEX_BIT
vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, ...);
```

## What's Next?

Push constants let us parameterize shaders at runtime. But what about compile-time parameters? In [Chapter 08](ch08_specialization.md), we'll learn about specialization constants for optimizing shader variants.

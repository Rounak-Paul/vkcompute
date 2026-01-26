# Chapter 08: Specialization Constants

## Overview

Specialization constants are compile-time values baked into shaders. This chapter covers:

- Defining specialization constants in shaders
- Setting values at pipeline creation
- Use cases and performance benefits

**What you'll learn:**

- When specialization beats runtime branching
- Creating shader variants efficiently
- Optimizing workgroup sizes

## Runtime vs Compile-Time

```glsl
// Runtime: Branch every invocation
if (mode == 0) { /* path A */ }
else { /* path B */ }

// Compile-time: Branch removed by compiler
layout(constant_id = 0) const uint MODE = 0;
if (MODE == 0) { /* path A - only this exists in binary */ }
else { /* path B - optimized away */ }
```

## Shader Declaration

```glsl
#version 450

// Specialization constants with default values
layout(constant_id = 0) const uint WORKGROUP_SIZE = 256;
layout(constant_id = 1) const uint OPERATION = 0;  // 0=add, 1=mul, 2=fma
layout(constant_id = 2) const float SCALE = 1.0;

layout(local_size_x_id = 0) in;  // Use constant 0 for workgroup size!

layout(set = 0, binding = 0) buffer Data { float v[]; } data;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    
    // Compiler optimizes away unused branches
    if (OPERATION == 0) {
        data.v[idx] = data.v[idx] + SCALE;
    } else if (OPERATION == 1) {
        data.v[idx] = data.v[idx] * SCALE;
    } else {
        data.v[idx] = fma(data.v[idx], SCALE, 1.0);
    }
}
```

Note: `local_size_x_id = 0` links workgroup size to specialization constant 0!

## Setting Values at Pipeline Creation

### Define the Map

```c
typedef struct {
    uint32_t workgroup_size;
    uint32_t operation;
    float scale;
} SpecConstants;

SpecConstants spec = {
    .workgroup_size = 512,
    .operation = 1,  // multiply
    .scale = 2.0f
};

VkSpecializationMapEntry entries[] = {
    {
        .constantID = 0,
        .offset = offsetof(SpecConstants, workgroup_size),
        .size = sizeof(uint32_t)
    },
    {
        .constantID = 1,
        .offset = offsetof(SpecConstants, operation),
        .size = sizeof(uint32_t)
    },
    {
        .constantID = 2,
        .offset = offsetof(SpecConstants, scale),
        .size = sizeof(float)
    }
};

VkSpecializationInfo spec_info = {
    .mapEntryCount = 3,
    .pMapEntries = entries,
    .dataSize = sizeof(spec),
    .pData = &spec
};
```

### Create Pipeline with Specialization

```c
VkPipelineShaderStageCreateInfo stage_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_COMPUTE_BIT,
    .module = shader_module,
    .pName = "main",
    .pSpecializationInfo = &spec_info  // <-- Here!
};

VkComputePipelineCreateInfo pipeline_info = {
    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    .stage = stage_info,
    .layout = pipeline_layout
};

vkCreateComputePipelines(device, cache, 1, &pipeline_info, NULL, &pipeline);
```

## Creating Shader Variants

Generate multiple pipelines with different constants:

```c
typedef struct {
    uint32_t workgroup_size;
    uint32_t operation;
} Variant;

Variant variants[] = {
    {64, 0},   // Small workgroup, add
    {256, 0},  // Medium workgroup, add
    {512, 0},  // Large workgroup, add
    {256, 1},  // Medium workgroup, multiply
    {256, 2},  // Medium workgroup, fma
};

VkPipeline pipelines[5];

for (int i = 0; i < 5; i++) {
    // Update spec data
    spec.workgroup_size = variants[i].workgroup_size;
    spec.operation = variants[i].operation;
    
    // Create pipeline (reuses shader module)
    vkCreateComputePipelines(device, cache, 1, &pipeline_info, 
                             NULL, &pipelines[i]);
}
```

## Optimal Workgroup Sizes

Different GPUs prefer different workgroup sizes:

| GPU Vendor | Optimal Size |
|------------|--------------|
| NVIDIA | 256, 512, 1024 |
| AMD | 64, 256 |
| Intel | 16, 32 |
| Apple | 256, 1024 |

Use specialization to create vendor-optimized pipelines:

```c
uint32_t optimal_size;
if (strstr(props.deviceName, "NVIDIA")) {
    optimal_size = 256;
} else if (strstr(props.deviceName, "AMD")) {
    optimal_size = 64;
} else {
    optimal_size = 128;  // Safe default
}

spec.workgroup_size = optimal_size;
```

## Running the Example

```bash
./build/bin/ch08_specialization
```

Expected output:
```
==============================================
  VKCompute - Chapter 08: Specialization Constants
==============================================

Creating shader variants...
  Add, WG=64
  Add, WG=256
  Add, WG=512
  Multiply, WG=256
  FMA, WG=256

Testing variants:
Input: a[5] = 5.0, b[5] = 100.0, scale = 2.0

  Add, WG=64:    result[5] = 105.0 ✓
  Add, WG=256:   result[5] = 105.0 ✓
  Add, WG=512:   result[5] = 105.0 ✓
  Multiply:      result[5] = 200.0 ✓
  FMA:           result[5] = 11.0 ✓

=== Performance by Workgroup Size ===
  WG=64:  0.42 ms
  WG=256: 0.31 ms  <-- Best for this GPU
  WG=512: 0.35 ms
```

## Supported Types

Specialization constants support:

| GLSL Type | SPIR-V Type |
|-----------|-------------|
| `bool` | `OpTypeBool` |
| `int` | `OpTypeInt` |
| `uint` | `OpTypeInt` |
| `float` | `OpTypeFloat` |
| `double` | `OpTypeFloat` |

!!! note "No Vectors/Matrices"
    Composite types aren't directly supported. Use separate constants:
    ```glsl
    layout(constant_id = 0) const float VEC_X = 0.0;
    layout(constant_id = 1) const float VEC_Y = 0.0;
    layout(constant_id = 2) const float VEC_Z = 0.0;
    ```

## Use Cases

### 1. Algorithm Selection

```glsl
layout(constant_id = 0) const uint ALGORITHM = 0;

if (ALGORITHM == 0) {
    // Fast approximate version
} else {
    // Slow accurate version
}
```

### 2. Feature Toggles

```glsl
layout(constant_id = 0) const bool USE_FAST_MATH = true;
layout(constant_id = 1) const bool DEBUG_OUTPUT = false;
```

### 3. Array Sizes

```glsl
layout(constant_id = 0) const uint KERNEL_SIZE = 5;
float kernel[KERNEL_SIZE];
```

### 4. Loop Unrolling

```glsl
layout(constant_id = 0) const uint ITERATIONS = 4;

// Compiler can unroll this loop
for (uint i = 0; i < ITERATIONS; i++) {
    // ...
}
```

## Specialization vs Push Constants

| Aspect | Specialization | Push Constants |
|--------|----------------|----------------|
| When set | Pipeline creation | Command recording |
| Changeable | No (need new pipeline) | Yes (per-dispatch) |
| Performance | Best (compiled in) | Good |
| Use for | Algorithm, sizes | Parameters, matrices |

## Exercises

1. **Auto-Tuning**: Create a benchmark that tests different workgroup sizes and selects the best.

2. **Feature Flags**: Implement debug visualization that can be compiled out in release.

3. **Kernel Variants**: Create convolution kernels with specialized sizes (3x3, 5x5, 7x7).

## Common Errors

### Default Value Used

Forgot to provide specialization info:
```c
stage_info.pSpecializationInfo = NULL;  // Uses shader defaults
```

### Constant ID Mismatch

Shader uses ID 0, but map entry uses ID 1:
```glsl
layout(constant_id = 0) const uint VALUE = 0;
```
```c
entry.constantID = 1;  // Wrong!
```

### Size Mismatch

Shader expects uint32, but you provide uint64:
```c
entry.size = sizeof(uint64_t);  // Wrong for uint constant
```

## What's Next?

We've optimized individual invocations. In [Chapter 09](ch09_subgroups.md), we'll learn about subgroups — cooperating groups of invocations that can communicate without barriers.

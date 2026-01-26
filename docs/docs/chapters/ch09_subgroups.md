# Chapter 09: Subgroups

## Overview

Subgroups (also called waves or warps) are groups of invocations that execute in lockstep. This chapter covers:

- Understanding the subgroup execution model
- Subgroup operations (vote, ballot, arithmetic)
- Efficient reductions and scans

**What you'll learn:**

- Hardware-level parallelism
- Avoiding shared memory for communication
- Writing efficient reduction kernels

## What is a Subgroup?

Within a workgroup, invocations are grouped into **subgroups** that execute together:

```
Workgroup (256 invocations)
├── Subgroup 0 (32 invocations) ─── Execute in lockstep
├── Subgroup 1 (32 invocations) ─── Execute in lockstep
├── Subgroup 2 (32 invocations) ─── Execute in lockstep
├── ...
└── Subgroup 7 (32 invocations) ─── Execute in lockstep
```

Subgroup size varies by GPU:

| Vendor | Subgroup Size |
|--------|---------------|
| NVIDIA | 32 (warp) |
| AMD | 32 or 64 (wave) |
| Intel | 8, 16, or 32 (SIMD width) |
| Apple | 32 |

## Querying Subgroup Properties

```c
VkPhysicalDeviceSubgroupProperties subgroup_props = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES
};

VkPhysicalDeviceProperties2 props2 = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
    .pNext = &subgroup_props
};

vkGetPhysicalDeviceProperties2(physical_device, &props2);

printf("Subgroup size: %u\n", subgroup_props.subgroupSize);
printf("Supported stages: %x\n", subgroup_props.supportedStages);
printf("Supported operations: %x\n", subgroup_props.supportedOperations);
```

## Required Extensions

```glsl
#version 450
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_KHR_shader_subgroup_ballot : require
```

## Built-in Variables

```glsl
// Your position within the subgroup (0 to subgroupSize-1)
uint gl_SubgroupInvocationID;

// Size of the subgroup
uint gl_SubgroupSize;

// Which subgroup you're in (within workgroup)
uint gl_SubgroupID;

// Total number of subgroups in workgroup
uint gl_NumSubgroups;
```

## Subgroup Operations

### Vote Operations

All invocations vote on a condition:

```glsl
bool condition = (value > threshold);

// True if all invocations have condition = true
bool all_true = subgroupAll(condition);

// True if any invocation has condition = true
bool any_true = subgroupAny(condition);

// True if all invocations have same value
bool all_equal = subgroupAllEqual(value);
```

### Broadcast

Share a value from one invocation to all:

```glsl
// Get value from invocation 0
float shared = subgroupBroadcastFirst(my_value);

// Get value from specific invocation
float from_5 = subgroupBroadcast(my_value, 5);
```

### Arithmetic Reductions

Reduce across the entire subgroup:

```glsl
float sum = subgroupAdd(my_value);      // Sum all values
float product = subgroupMul(my_value);  // Multiply all
float minimum = subgroupMin(my_value);  // Find minimum
float maximum = subgroupMax(my_value);  // Find maximum
```

### Inclusive/Exclusive Scans

Prefix operations:

```glsl
// Inclusive: includes current invocation
float prefix_sum = subgroupInclusiveAdd(my_value);
// Invocation 0: v0
// Invocation 1: v0 + v1
// Invocation 2: v0 + v1 + v2
// ...

// Exclusive: excludes current invocation
float exclusive_sum = subgroupExclusiveAdd(my_value);
// Invocation 0: 0
// Invocation 1: v0
// Invocation 2: v0 + v1
// ...
```

### Ballot

Get a bitmask of which invocations satisfy a condition:

```glsl
bool condition = (my_value > 0);
uvec4 ballot = subgroupBallot(condition);

// Count how many invocations satisfy condition
uint count = subgroupBallotBitCount(ballot);
```

### Shuffle

Exchange values between invocations:

```glsl
// Get value from invocation at delta offset
float neighbor = subgroupShuffleDown(my_value, 1);  // From next invocation
float prev = subgroupShuffleUp(my_value, 1);        // From previous
float specific = subgroupShuffle(my_value, 5);       // From invocation 5
```

## Example: Parallel Reduction

Sum 1 million values efficiently:

```glsl
#version 450
#extension GL_KHR_shader_subgroup_arithmetic : require

layout(local_size_x = 256) in;

layout(set = 0, binding = 0) readonly buffer Input {
    float data[];
} input_buf;

layout(set = 0, binding = 1) buffer Output {
    float data[];
} output_buf;

shared float shared_data[8];  // One per subgroup

void main() {
    uint idx = gl_GlobalInvocationID.x;
    float value = input_buf.data[idx];
    
    // Step 1: Reduce within subgroup (no shared memory needed!)
    float subgroup_sum = subgroupAdd(value);
    
    // Step 2: First invocation of each subgroup writes to shared
    if (gl_SubgroupInvocationID == 0) {
        shared_data[gl_SubgroupID] = subgroup_sum;
    }
    
    barrier();
    
    // Step 3: First subgroup reduces the partial sums
    if (gl_SubgroupID == 0) {
        float partial = (gl_SubgroupInvocationID < gl_NumSubgroups) 
                        ? shared_data[gl_SubgroupInvocationID] 
                        : 0.0;
        float workgroup_sum = subgroupAdd(partial);
        
        if (gl_SubgroupInvocationID == 0) {
            output_buf.data[gl_WorkGroupID.x] = workgroup_sum;
        }
    }
}
```

## Running the Example

```bash
./build/bin/ch09_subgroups
```

Expected output:
```
==============================================
  VKCompute - Chapter 09: Subgroups
==============================================

=== Subgroup Properties ===
  Subgroup Size: 32
  Supported Stages: COMPUTE FRAGMENT
  Supported Operations:
    - Basic (elect, barrier)
    - Vote (all, any, equal)
    - Arithmetic (add, mul, min, max)
    - Ballot
    - Shuffle
    - Shuffle Relative

=== Reduction Benchmark ===
Array size: 1,048,576 elements
Expected sum: 549755289600.0

Results:
  Computed sum: 549755289600.0 ✓
  Time: 2.89 ms
  Throughput: 3.34 GB/s
  Elements/second: 0.90 billion
```

## Performance Comparison

| Method | Time | Notes |
|--------|------|-------|
| CPU (single thread) | 2.1 ms | Baseline |
| GPU (shared memory) | 0.8 ms | Classic approach |
| GPU (subgroups) | 0.3 ms | 2-3x faster! |

Subgroups are faster because:
- No shared memory writes/reads
- No barriers within subgroup
- Hardware-optimized operations

## Subgroup Barriers

Sometimes you need explicit synchronization:

```glsl
// Wait for all subgroup invocations
subgroupBarrier();

// Memory-specific barriers
subgroupMemoryBarrier();           // All memory
subgroupMemoryBarrierBuffer();     // Buffer memory
subgroupMemoryBarrierShared();     // Shared memory
subgroupMemoryBarrierImage();      // Image memory
```

## Elect Operation

Choose one invocation to do special work:

```glsl
if (subgroupElect()) {
    // Only one invocation per subgroup executes this
    atomicAdd(counter, 1);
}
```

## Exercises

1. **Parallel Max**: Implement finding the maximum value using subgroups.

2. **Prefix Sum**: Implement an exclusive prefix sum (scan) for a large array.

3. **Histogram**: Use subgroup ballot to count values in ranges.

## Common Errors

### Extension Not Enabled

```glsl
// Error: subgroupAdd not found
#version 450
// Missing: #extension GL_KHR_shader_subgroup_arithmetic : require
```

### Assuming Subgroup Size

```glsl
// Wrong: Assumes 32
if (gl_SubgroupInvocationID < 32) { ... }

// Right: Use actual size
if (gl_SubgroupInvocationID < gl_SubgroupSize) { ... }
```

### Cross-Subgroup Communication

Subgroup operations only work within a subgroup:
```glsl
// This won't share data between subgroups!
float wrong = subgroupBroadcast(my_value, 40);  // Invalid if size is 32
```

## Feature Support

Check what your GPU supports:

```c
VkSubgroupFeatureFlags ops = subgroup_props.supportedOperations;

if (ops & VK_SUBGROUP_FEATURE_BASIC_BIT)      printf("Basic\n");
if (ops & VK_SUBGROUP_FEATURE_VOTE_BIT)       printf("Vote\n");
if (ops & VK_SUBGROUP_FEATURE_ARITHMETIC_BIT) printf("Arithmetic\n");
if (ops & VK_SUBGROUP_FEATURE_BALLOT_BIT)     printf("Ballot\n");
if (ops & VK_SUBGROUP_FEATURE_SHUFFLE_BIT)    printf("Shuffle\n");
```

## What's Next?

We've covered the major compute concepts. In [Chapter 10](ch10_debugging.md), we'll learn how to debug and profile Vulkan applications effectively.

# Chapter 10: Debugging

## Overview

Debugging GPU code is challenging. This chapter covers:

- Validation layers for error detection
- Debug naming and labels
- Profiling and performance analysis
- Common debugging techniques

**What you'll learn:**

- Setting up validation layers
- Using GPU debugging tools
- Identifying performance bottlenecks

## Validation Layers

Vulkan has minimal runtime error checking for performance. **Validation layers** add comprehensive checking during development.

### Enabling Validation

```c
const char* layers[] = {
    "VK_LAYER_KHRONOS_validation"
};

VkInstanceCreateInfo create_info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .enabledLayerCount = 1,
    .ppEnabledLayerNames = layers,
    .enabledExtensionCount = 1,
    .ppEnabledExtensionNames = (const char*[]){
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    }
};
```

### Debug Callback

Receive validation messages:

```c
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* user_data) 
{
    const char* severity_str = "";
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        severity_str = "ERROR";
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        severity_str = "WARNING";
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        severity_str = "INFO";
    
    fprintf(stderr, "[Vulkan %s] %s\n", severity_str, data->pMessage);
    
    return VK_FALSE;  // Don't abort
}

// Create messenger
VkDebugUtilsMessengerCreateInfoEXT messenger_info = {
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
    .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
    .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
    .pfnUserCallback = debug_callback
};

VkDebugUtilsMessengerEXT messenger;
// Need to load function pointer
PFN_vkCreateDebugUtilsMessengerEXT func = 
    (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");
func(instance, &messenger_info, NULL, &messenger);
```

## Debug Object Names

Name your objects for clearer error messages:

```c
VkDebugUtilsObjectNameInfoEXT name_info = {
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
    .objectType = VK_OBJECT_TYPE_BUFFER,
    .objectHandle = (uint64_t)input_buffer,
    .pObjectName = "Input Data Buffer"
};

PFN_vkSetDebugUtilsObjectNameEXT setName = 
    (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(
        device, "vkSetDebugUtilsObjectNameEXT");
setName(device, &name_info);
```

Now errors reference "Input Data Buffer" instead of "VkBuffer 0x7f3a2b1c":

```
[Vulkan ERROR] Buffer "Input Data Buffer" is being used without proper synchronization
```

## Debug Labels

Mark regions in command buffers:

```c
// Helper function
void cmd_begin_label(VkCommandBuffer cmd, const char* name, 
                     float r, float g, float b) {
    VkDebugUtilsLabelEXT label = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pLabelName = name,
        .color = {r, g, b, 1.0f}
    };
    
    PFN_vkCmdBeginDebugUtilsLabelEXT func = ...;
    func(cmd, &label);
}

void cmd_end_label(VkCommandBuffer cmd) {
    PFN_vkCmdEndDebugUtilsLabelEXT func = ...;
    func(cmd);
}

// Usage
cmd_begin_label(cmd, "Data Processing", 0.0f, 1.0f, 0.0f);
vkCmdBindPipeline(cmd, ...);
vkCmdDispatch(cmd, 256, 1, 1);
cmd_end_label(cmd);

cmd_begin_label(cmd, "Post-Processing", 1.0f, 0.0f, 0.0f);
// ...
cmd_end_label(cmd);
```

These labels appear in debugging tools like RenderDoc.

## GPU Timestamps

Measure GPU execution time:

```c
// Create query pool
VkQueryPoolCreateInfo query_info = {
    .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
    .queryType = VK_QUERY_TYPE_TIMESTAMP,
    .queryCount = 2
};

VkQueryPool query_pool;
vkCreateQueryPool(device, &query_info, NULL, &query_pool);

// Record timestamps
vkCmdResetQueryPool(cmd, query_pool, 0, 2);
vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
                    query_pool, 0);

vkCmdDispatch(cmd, 256, 1, 1);

vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
                    query_pool, 1);

// After execution, read results
uint64_t timestamps[2];
vkGetQueryPoolResults(device, query_pool, 0, 2, 
                      sizeof(timestamps), timestamps, 
                      sizeof(uint64_t),
                      VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

float nano_per_tick = props.limits.timestampPeriod;
float ms = (timestamps[1] - timestamps[0]) * nano_per_tick / 1e6;
printf("Dispatch took %.3f ms\n", ms);
```

## Pipeline Statistics

Query execution statistics:

```c
VkQueryPoolCreateInfo query_info = {
    .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
    .queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS,
    .queryCount = 1,
    .pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT
};

// Wrap dispatch in query
vkCmdBeginQuery(cmd, query_pool, 0, 0);
vkCmdDispatch(cmd, 256, 1, 1);
vkCmdEndQuery(cmd, query_pool, 0);

// Read results
uint64_t invocations;
vkGetQueryPoolResults(device, query_pool, 0, 1, 
                      sizeof(invocations), &invocations,
                      sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
printf("Shader invocations: %lu\n", invocations);
```

## Debugging Tools

### RenderDoc

Free, open-source GPU debugger:

1. Launch your app through RenderDoc
2. Capture a frame
3. Inspect:
   - Command buffer contents
   - Buffer data at each stage
   - Shader debugging (step through execution)

### NVIDIA Nsight

For NVIDIA GPUs:
- Nsight Graphics: Frame debugging
- Nsight Compute: Kernel profiling
- Nsight Systems: System-wide profiling

### AMD RGP/RGD

For AMD GPUs:
- Radeon GPU Profiler (RGP): Performance analysis
- Radeon GPU Detective (RGD): Crash debugging

### Intel GPA

For Intel GPUs:
- Graphics Performance Analyzers
- Metrics and trace analysis

## Common Validation Errors

### Synchronization Hazard

```
SYNC-HAZARD-WRITE-AFTER-READ: vkCmdDispatch(): Hazard WRITE_AFTER_READ
for Buffer "Output Buffer". Prior access by vkCmdDispatch() with usage READ.
```

**Fix**: Add a barrier between dispatches.

### Missing Descriptor

```
VUID-vkCmdDispatch-None-02699: Descriptor set 0 binding 2 is not bound.
```

**Fix**: Update all required descriptor bindings.

### Invalid Layout

```
VUID-VkWriteDescriptorSet-descriptorType-00327: descriptorType is 
VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER but pBufferInfo[0].buffer was 
created with VK_BUFFER_USAGE_STORAGE_BUFFER_BIT.
```

**Fix**: Match buffer usage to descriptor type.

## Printf Debugging

Some drivers support printf from shaders:

```glsl
#extension GL_EXT_debug_printf : enable

void main() {
    debugPrintfEXT("Invocation %u: value = %f\n", 
                   gl_GlobalInvocationID.x, my_value);
}
```

Enable with:
```bash
export VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT
```

!!! warning "Performance"
    Printf severely impacts performance. Use only for debugging specific issues.

## Running the Example

```bash
./build/bin/ch10_debugging
```

Expected output:
```
==============================================
  VKCompute - Chapter 10: Debugging
==============================================

=== Validation Layers ===
Validation layers enabled

=== Object Naming ===
Named all objects for debugging

=== Timestamp Query ===
Dispatch 1: 0.127 ms
Dispatch 2: 0.089 ms  
Dispatch 3: 0.091 ms

=== Triggering Validation Errors ===
[Vulkan ERROR] Missing barrier between dispatches
(This error is intentional for demonstration)

=== Debug Tips ===
  - Enable validation layers during development
  - Name all objects for clearer error messages
  - Use RenderDoc for GPU debugging
  - Profile with vendor tools
```

## Best Practices

### During Development

- ✅ Always enable validation layers
- ✅ Name all objects
- ✅ Use debug labels for command sections
- ✅ Check return values

### For Release

- ❌ Disable validation layers (performance)
- ✅ Keep error handling
- ✅ Add telemetry for crash reporting

## Performance Debugging Checklist

1. **Are dispatches too small?** Increase work per dispatch
2. **Too many barriers?** Batch independent work
3. **Memory bandwidth bound?** Use shared memory
4. **Occupancy limited?** Reduce register usage
5. **Pipeline switching overhead?** Batch by pipeline

## Exercises

1. **Custom Callback**: Log validation messages to a file with timestamps.

2. **Benchmark Suite**: Create automated performance tests with timestamp queries.

3. **Error Injection**: Intentionally trigger different validation errors to learn the messages.

## Congratulations!

You've completed the VKCompute tutorial series! You now understand:

- Vulkan initialization and device setup
- Buffer management and memory types
- Descriptor sets and resource binding
- Pipeline creation and management
- GPU synchronization
- Push constants and specialization
- Subgroup operations
- Debugging and profiling

## Where to Go Next

- **Vulkan Specification**: The authoritative reference
- **Vulkan Guide**: Practical patterns and best practices
- **GPU Gems**: Advanced compute techniques
- **Vendor Documentation**: NVIDIA, AMD, Intel optimization guides

Happy computing! 🚀

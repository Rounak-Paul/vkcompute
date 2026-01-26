# VKCompute Tutorial Series

Welcome to the **VKCompute Tutorial Series** — a hands-on guide to learning Vulkan Compute programming from the ground up.

## What You'll Learn

This series takes you from zero to proficient in Vulkan Compute through 10 practical chapters:

| Chapter | Topic | What You'll Build |
|---------|-------|-------------------|
| 01 | Hello Compute | Your first Vulkan program |
| 02 | Buffers | GPU memory management |
| 03 | Descriptors | Shader resource binding |
| 04 | Pipelines | Compute pipeline creation |
| 05 | Synchronization | GPU/CPU coordination |
| 06 | Memory | Advanced memory strategies |
| 07 | Push Constants | Fast parameter passing |
| 08 | Specialization | Compile-time optimization |
| 09 | Subgroups | Wave-level parallelism |
| 10 | Debugging | Validation and profiling |

## Why Vulkan Compute?

Vulkan Compute gives you **direct control over the GPU** for general-purpose computing:

- **Cross-platform**: Works on Windows, Linux, macOS, Android, and more
- **High performance**: Minimal driver overhead, explicit control
- **Modern**: Designed for today's parallel hardware
- **Industry standard**: Backed by Khronos Group

## Prerequisites

Before starting, you should have:

- Basic C programming knowledge
- Understanding of pointers and memory
- Familiarity with command line/terminal
- No prior GPU programming experience required!

## Quick Start

```bash
# Clone the repository
git clone https://github.com/vkcompute/vkcompute.git
cd vkcompute

# Build all chapters
./build.sh  # Linux/macOS
build.bat   # Windows

# Run your first program
./build/bin/ch01_hello_compute
```

## Project Structure

```
vkcompute/
├── chapters/           # Tutorial source code
│   ├── ch01_hello_compute/
│   ├── ch02_buffers/
│   └── ...
├── vkc/               # Shared helper library
│   ├── include/       # Headers (vk_init.h, vk_utils.h, file_utils.h)
│   └── src/           # Implementation files
├── docs/              # This documentation
└── build/             # Compiled binaries
```

## The vkc Helper Library

Chapter 1 uses raw Vulkan API to show every detail of initialization. From **Chapter 2 onwards**, we use the `vkc` helper library to reduce boilerplate so we can focus on the concepts being taught.

The library provides:

| Header | Purpose |
|--------|--------|
| `vk_init.h` | `VkcContext` struct, instance/device creation, buffer helpers |
| `vk_utils.h` | Shader loading, descriptor helpers, debug utilities |
| `file_utils.h` | File I/O, path resolution relative to executable |

Key types and functions:

```c
// Context holds all Vulkan state
VkcContext ctx;
vkc_init(&ctx, NULL);  // Initialize Vulkan

// Buffer creation with automatic memory allocation
VkcBuffer buffer;
vkc_create_buffer(&ctx, size, usage, memory_flags, &buffer);
vkc_upload_buffer(&ctx, &buffer, data, size);

// Shader loading
VkShaderModule shader;
vkc_load_shader(&ctx, "shader.comp.spv", &shader);
```

!!! note "Educational Focus"
    The vkc library is intentionally simple and explicit. It's not a production framework — it's designed to be readable so you can see exactly what Vulkan calls are being made.

## Ready to Begin?

Start with the [Introduction](introduction.md) for an overview of Vulkan concepts, or jump straight to [Chapter 01](chapters/ch01_hello_compute.md) if you prefer learning by doing.

!!! tip "Learning Approach"
    Each chapter builds on the previous one. We recommend going through them in order, but feel free to skip ahead if you're already familiar with certain concepts.

# VKCompute - Vulkan Compute Video Series

A cross-platform C project for learning Vulkan Compute through a video series.

## Prerequisites

### All Platforms
- [Vulkan SDK](https://vulkan.lunarg.com/sdk/home) (1.2 or later)
- [CMake](https://cmake.org/) (3.16 or later)
- C11 compatible compiler

### Linux
```bash
# Ubuntu/Debian
sudo apt install vulkan-tools libvulkan-dev vulkan-validationlayers

# Fedora
sudo dnf install vulkan-tools vulkan-loader-devel vulkan-validation-layers

# Arch
sudo pacman -S vulkan-tools vulkan-headers vulkan-validation-layers
```

### Windows
- Visual Studio 2019+ with C/C++ workload
- Install Vulkan SDK from LunarG

### macOS
- Xcode Command Line Tools
- Install Vulkan SDK from LunarG (MoltenVK)

## Building

### Linux/macOS
```bash
# Clone and build
cd vkcompute
chmod +x build.sh
./build.sh

# Or manually:
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --parallel
```

### Windows
```batch
# Using build script
build.bat

# Or manually with Visual Studio:
mkdir build
cd build
cmake -G "Visual Studio 17 2022" ..
cmake --build . --config Release
```

## Project Structure

```
vkcompute/
├── CMakeLists.txt          # Root CMake configuration
├── build.sh                # Linux/macOS build script
├── build.bat               # Windows build script
├── common/                 # Shared utility library
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── vk_init.h       # Vulkan initialization
│   │   ├── vk_utils.h      # Utility macros and functions
│   │   └── file_utils.h    # File I/O helpers
│   └── src/
│       ├── vk_init.c
│       ├── vk_utils.c
│       └── file_utils.c
├── episodes/               # Each video episode
│   ├── ep01_hello_compute/
│   │   ├── CMakeLists.txt
│   │   ├── main.c
│   │   └── shaders/
│   │       └── double.comp
│   ├── ep02_buffers/       # (future)
│   └── ...
└── build/                  # Build output (generated)
    └── bin/                # Executables
        └── shaders/        # Compiled SPIR-V shaders
```

## Running Episodes

After building, executables are in `build/bin/`:

```bash
# Linux/macOS
./build/bin/ep01_hello_compute

# Windows
build\bin\Release\ep01_hello_compute.exe
```

## Episode Guide

| Episode | Topic | Description |
|---------|-------|-------------|
| 01 | Hello Compute | First compute shader, buffers, dispatch |
| 02 | Buffers | Device/host memory, staging buffers |
| 03 | Descriptors | Descriptor sets, layouts, pools |
| 04 | Pipelines | Pipeline caching, specialization |
| 05 | Synchronization | Fences, semaphores, barriers |
| 06 | Memory | Memory types, allocation strategies |
| 07 | Push Constants | Small data without descriptors |
| 08 | Specialization | Compile-time shader constants |
| 09 | Subgroups | Wave/warp-level operations |
| 10 | Debugging | Validation layers, profiling |

## Adding New Episodes

1. Create a new directory: `episodes/epXX_topic_name/`
2. Add `CMakeLists.txt`:
```cmake
set(EPISODE_NAME epXX_topic_name)

add_executable(${EPISODE_NAME}
    main.c
)

target_link_libraries(${EPISODE_NAME} PRIVATE
    vkcompute_common
)

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${EPISODE_NAME})
compile_shaders(${EPISODE_NAME} ${SHADER_DIR} ${SHADER_OUTPUT_DIR})
```

3. Uncomment in root `CMakeLists.txt`:
```cmake
add_subdirectory(episodes/epXX_topic_name)
```

## Common Library API

### Initialization
```c
VkcContext ctx;
VkcConfig config = vkc_config_default();
vkc_init(&ctx, &config);
// ... use Vulkan ...
vkc_cleanup(&ctx);
```

### Buffers
```c
VkcBuffer buffer;
vkc_create_buffer(&ctx, size, usage, mem_props, &buffer);
vkc_upload_buffer(&ctx, &buffer, data, size);
vkc_download_buffer(&ctx, &buffer, data, size);
vkc_destroy_buffer(&ctx, &buffer);
```

### Shaders
```c
VkShaderModule shader;
vkc_load_shader(&ctx, "shader.comp.spv", &shader);
vkc_create_compute_pipeline(&ctx, shader, layout, &pipeline);
```

## License

See [LICENSE](LICENSE) file.

## Related

Based on learnings from the CUDA video series. This project brings similar concepts to the Vulkan compute ecosystem.

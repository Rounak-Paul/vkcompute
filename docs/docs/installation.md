# Installation

## Prerequisites

### All Platforms

1. **Vulkan SDK** (1.2 or later) - [Download from LunarG](https://vulkan.lunarg.com/sdk/home)
2. **CMake** (3.16 or later) - [Download from cmake.org](https://cmake.org/download/)
3. **C11 compatible compiler**

### Linux

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install vulkan-tools libvulkan-dev vulkan-validationlayers cmake build-essential

# Fedora
sudo dnf install vulkan-tools vulkan-loader-devel vulkan-validation-layers cmake gcc

# Arch
sudo pacman -S vulkan-tools vulkan-headers vulkan-validation-layers cmake base-devel
```

Verify installation:
```bash
vulkaninfo | head -20
```

### Windows

1. Install [Visual Studio 2019+](https://visualstudio.microsoft.com/) with "Desktop development with C++" workload
2. Install [Vulkan SDK](https://vulkan.lunarg.com/sdk/home) from LunarG
3. Ensure `glslc` is in your PATH (included with Vulkan SDK)

Verify installation:
```batch
vulkaninfo
```

### macOS

1. Install Xcode Command Line Tools:
   ```bash
   xcode-select --install
   ```

2. Install [Vulkan SDK](https://vulkan.lunarg.com/sdk/home) from LunarG (includes MoltenVK)

3. Set environment variables (add to `~/.zshrc` or `~/.bash_profile`):
   ```bash
   export VULKAN_SDK=/path/to/vulkansdk/macOS
   export PATH=$VULKAN_SDK/bin:$PATH
   export DYLD_LIBRARY_PATH=$VULKAN_SDK/lib:$DYLD_LIBRARY_PATH
   export VK_ICD_FILENAMES=$VULKAN_SDK/share/vulkan/icd.d/MoltenVK_icd.json
   export VK_LAYER_PATH=$VULKAN_SDK/share/vulkan/explicit_layer.d
   ```

Verify installation:
```bash
vulkaninfo | head -20
```

## Building VKCompute

### Clone the Repository

```bash
git clone https://github.com/vkcompute/vkcompute.git
cd vkcompute
```

### Linux/macOS

```bash
# Using the build script
chmod +x build.sh
./build.sh

# Or manually
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --parallel
```

### Windows

```batch
# Using the build script
build.bat

# Or with Visual Studio
mkdir build
cd build
cmake -G "Visual Studio 17 2022" ..
cmake --build . --config Release
```

## Verify Installation

Run the first chapter:

```bash
# Linux/macOS
./build/bin/ch01_hello_compute

# Windows
build\bin\Release\ch01_hello_compute.exe
```

You should see output like:
```
==============================================
  VKCompute - Chapter 01: Hello Vulkan Compute
==============================================

✓ Vulkan instance created
✓ Found 1 GPU(s)

  GPU 0: NVIDIA GeForce RTX 3080
    Type: Discrete GPU
    API Version: 1.3.277
    ...

✓ Logical device created
✓ Compute queue retrieved

=== SUCCESS ===
Vulkan is initialized and ready for compute work!
```

## Troubleshooting

### "No GPUs with Vulkan support found"

- Ensure your GPU drivers are up to date
- Check that Vulkan SDK is properly installed
- On macOS, verify MoltenVK environment variables are set

### "Failed to create Vulkan instance" (Error -9 on macOS)

This is `VK_ERROR_INCOMPATIBLE_DRIVER`. The portability extension is needed for MoltenVK. Our code handles this automatically, but ensure you have the latest Vulkan SDK.

### "Validation layer not found"

Install validation layers:
```bash
# Ubuntu/Debian
sudo apt install vulkan-validationlayers

# Fedora
sudo dnf install vulkan-validation-layers
```

### Shader Compilation Errors

Ensure `glslc` (from the Vulkan SDK) is in your PATH:
```bash
which glslc  # Should show path to glslc
```

## IDE Setup

### Visual Studio Code

Install these extensions:

- **C/C++** (Microsoft)
- **CMake Tools** (Microsoft)
- **Shader languages support** for GLSL syntax highlighting

### CLion

CMake projects are natively supported. Just open the project folder.

### Visual Studio

Use "Open Folder" and select the project root. Visual Studio will detect CMakeLists.txt automatically.

## Next Steps

With everything installed, head to [Chapter 01](chapters/ch01_hello_compute.md) to write your first Vulkan program!

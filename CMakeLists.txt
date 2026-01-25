# VKCompute - Vulkan Compute Video Series
# Cross-platform CMake build system

cmake_minimum_required(VERSION 3.16)
project(VKCompute VERSION 1.0.0 LANGUAGES C)

# ============================================================================
# Build Configuration
# ============================================================================

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Build type (default to Release)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")

# ============================================================================
# Platform Detection
# ============================================================================

if(WIN32)
    set(PLATFORM_NAME "Windows")
    add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)
elseif(APPLE)
    set(PLATFORM_NAME "macOS")
    add_definitions(-DVK_USE_PLATFORM_MACOS_MVK)
    # MoltenVK for macOS
    find_library(MOLTENVK_LIBRARY MoltenVK HINTS /usr/local/lib $ENV{VULKAN_SDK}/lib)
elseif(UNIX)
    set(PLATFORM_NAME "Linux")
    add_definitions(-DVK_USE_PLATFORM_XCB_KHR)
endif()

message(STATUS "Platform: ${PLATFORM_NAME}")

# ============================================================================
# Find Vulkan
# ============================================================================

find_package(Vulkan REQUIRED)

if(Vulkan_FOUND)
    message(STATUS "Vulkan found!")
    message(STATUS "  Vulkan include: ${Vulkan_INCLUDE_DIRS}")
    message(STATUS "  Vulkan library: ${Vulkan_LIBRARIES}")
    if(Vulkan_glslc_FOUND)
        message(STATUS "  glslc: ${Vulkan_GLSLC_EXECUTABLE}")
    endif()
    if(Vulkan_glslangValidator_FOUND)
        message(STATUS "  glslangValidator: ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}")
    endif()
else()
    message(FATAL_ERROR "Vulkan not found! Please install Vulkan SDK.")
endif()

# ============================================================================
# Compiler Warnings
# ============================================================================

if(MSVC)
    add_compile_options(/W4 /WX-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    else()
        add_compile_options(-O2)
    endif()
endif()

# ============================================================================
# Shader Compilation Function
# ============================================================================

# Find shader compiler
find_program(GLSLC glslc HINTS ${Vulkan_GLSLC_EXECUTABLE} $ENV{VULKAN_SDK}/bin)
find_program(GLSLANG glslangValidator HINTS ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} $ENV{VULKAN_SDK}/bin)

if(GLSLC)
    set(SHADER_COMPILER ${GLSLC})
    set(SHADER_COMPILER_NAME "glslc")
elseif(GLSLANG)
    set(SHADER_COMPILER ${GLSLANG})
    set(SHADER_COMPILER_NAME "glslangValidator")
else()
    message(WARNING "No GLSL compiler found. Shaders will not be compiled automatically.")
endif()

# Function to compile shaders
function(compile_shaders TARGET_NAME SHADER_DIR OUTPUT_DIR)
    if(NOT SHADER_COMPILER)
        message(WARNING "No shader compiler found. Skipping shader compilation for ${TARGET_NAME}")
        return()
    endif()

    file(GLOB SHADER_FILES 
        "${SHADER_DIR}/*.comp"
        "${SHADER_DIR}/*.vert"
        "${SHADER_DIR}/*.frag"
    )
    
    set(SPIRV_FILES)
    
    foreach(SHADER ${SHADER_FILES})
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        set(SPIRV_OUTPUT "${OUTPUT_DIR}/${SHADER_NAME}.spv")
        
        if(SHADER_COMPILER_NAME STREQUAL "glslc")
            add_custom_command(
                OUTPUT ${SPIRV_OUTPUT}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
                COMMAND ${SHADER_COMPILER} ${SHADER} -o ${SPIRV_OUTPUT}
                DEPENDS ${SHADER}
                COMMENT "Compiling shader: ${SHADER_NAME}"
            )
        else()
            add_custom_command(
                OUTPUT ${SPIRV_OUTPUT}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
                COMMAND ${SHADER_COMPILER} -V ${SHADER} -o ${SPIRV_OUTPUT}
                DEPENDS ${SHADER}
                COMMENT "Compiling shader: ${SHADER_NAME}"
            )
        endif()
        
        list(APPEND SPIRV_FILES ${SPIRV_OUTPUT})
    endforeach()
    
    add_custom_target(${TARGET_NAME}_shaders DEPENDS ${SPIRV_FILES})
    add_dependencies(${TARGET_NAME} ${TARGET_NAME}_shaders)
endfunction()

# ============================================================================
# Common Library
# ============================================================================

add_subdirectory(common)

# ============================================================================
# Episodes - Each video gets its own executable
# ============================================================================

# Episode 01: Hello Vulkan Compute
add_subdirectory(episodes/ep01_hello_compute)

# Episode 02: Buffers - Device/host memory, staging
add_subdirectory(episodes/ep02_buffers)

# Episode 03: Descriptors - Sets, layouts, pools
add_subdirectory(episodes/ep03_descriptors)

# Episode 04: Pipelines - Caching, management
add_subdirectory(episodes/ep04_pipelines)

# Episode 05: Synchronization - Fences, barriers
add_subdirectory(episodes/ep05_synchronization)

# Episode 06: Memory - Types, allocation strategies
add_subdirectory(episodes/ep06_memory)

# Episode 07: Push Constants - Small data transfer
add_subdirectory(episodes/ep07_push_constants)

# Episode 08: Specialization Constants
add_subdirectory(episodes/ep08_specialization)

# Episode 09: Subgroups - Wave/warp operations
add_subdirectory(episodes/ep09_subgroups)

# Episode 10: Debugging - Validation, profiling
add_subdirectory(episodes/ep10_debugging)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "=== VKCompute Build Configuration ===")
message(STATUS "Platform:       ${PLATFORM_NAME}")
message(STATUS "Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "C Standard:     C${CMAKE_C_STANDARD}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
